#include "hal/INetwork.h"
#include "logic/schedule_fetcher.h"

#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <unity.h>
#include <vector>

using namespace bustaferl;

namespace {

class FakeNet : public INetwork {
public:
  std::vector<std::string> urls_seen;
  // url-substring → body to return. First substring match wins.
  std::vector<std::pair<std::string, std::string>> routes;
  bool fail_all = false;

  bool connect(unsigned) override { return true; }
  bool isConnected() override { return true; }
  bool httpGet(const std::string &url, std::string &out) override {
    urls_seen.push_back(url);
    if (fail_all)
      return false;
    for (const auto &r : routes) {
      if (url.find(r.first) != std::string::npos) {
        out = r.second;
        return true;
      }
    }
    return false;
  }
};

time_t makeLocal(int year, int month, int day, int hour, int minute) {
  struct tm tm{};
  tm.tm_year = year - 1900;
  tm.tm_mon = month - 1;
  tm.tm_mday = day;
  tm.tm_hour = hour;
  tm.tm_min = minute;
  tm.tm_isdst = -1;
  return mktime(&tm);
}

const char *kTullResponse = R"JSON({
  "departureList": [
    { "dateTime": { "year":"2026","month":"5","day":"16","hour":"23","minute":"50" },
      "servingLine": { "number":"58A", "direction":"Wien Atzgersdorf" } },
    { "dateTime": { "year":"2026","month":"5","day":"17","hour":"5","minute":"6" },
      "servingLine": { "number":"58A", "direction":"Wien Atzgersdorf" } },
    { "dateTime": { "year":"2026","month":"5","day":"17","hour":"5","minute":"30" },
      "servingLine": { "number":"58A", "direction":"Wien Atzgersdorf" } }
  ]
})JSON";

const char *kSuedResponse = R"JSON({
  "departureList": [
    { "dateTime": { "year":"2026","month":"5","day":"17","hour":"4","minute":"50" },
      "servingLine": { "number":"U1", "direction":"Leopoldau" } },
    { "dateTime": { "year":"2026","month":"5","day":"17","hour":"5","minute":"10" },
      "servingLine": { "number":"U1", "direction":"Oberlaa" } }
  ]
})JSON";

ScheduleFetchConfig makeCfg() {
  ScheduleFetchConfig cfg;
  cfg.endpoint_base = "http://test.invalid/dm?fixed=1";
  cfg.limit = 50;
  cfg.query_hour = 22;
  cfg.query_minute = 0;
  cfg.cutoff_hour = 3;
  return cfg;
}

} // namespace

void setUp() {
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();
}
void tearDown() {}

void test_buildEfaUrl_encodes_date_and_diva() {
  time_t t = makeLocal(2026, 5, 16, 22, 0);
  std::string url = buildEfaUrl("http://example/dm?x=1", 60201395, t, 50);
  TEST_ASSERT_NOT_NULL(strstr(url.c_str(), "name_dm=60201395"));
  TEST_ASSERT_NOT_NULL(strstr(url.c_str(), "itdDateDay=16"));
  TEST_ASSERT_NOT_NULL(strstr(url.c_str(), "itdDateMonth=05"));
  TEST_ASSERT_NOT_NULL(strstr(url.c_str(), "itdDateYear=2026"));
  TEST_ASSERT_NOT_NULL(strstr(url.c_str(), "itdTimeHour=22"));
  TEST_ASSERT_NOT_NULL(strstr(url.c_str(), "itdTimeMinute=00"));
  TEST_ASSERT_NOT_NULL(strstr(url.c_str(), "limit=50"));
}

void test_computeCutoff_returns_next_local_03_00() {
  // At 22:00 local on 16 May → cutoff = 03:00 local on 17 May.
  time_t now = makeLocal(2026, 5, 16, 22, 0);
  time_t cut = computeCutoff(now, 3);
  TEST_ASSERT_EQUAL_INT64(makeLocal(2026, 5, 17, 3, 0), cut);
}

void test_computeCutoff_after_midnight_still_picks_next_03_00() {
  // 02:30 local on 17 May → cutoff = 03:00 local on 18 May (the *next* one).
  // Concept §12.3 only ever wants to look forward, never back into a partly
  // elapsed cutoff.
  time_t now = makeLocal(2026, 5, 17, 2, 30);
  time_t cut = computeCutoff(now, 3);
  TEST_ASSERT_EQUAL_INT64(makeLocal(2026, 5, 18, 3, 0), cut);
}

void test_fetchSchedule_one_diva_two_streams() {
  FakeNet net;
  net.routes.emplace_back("name_dm=60201395", kTullResponse);

  ScheduleStreamFilter f[STREAM_COUNT];
  f[STREAM_58A_ATZ] = {60201395, "58A", "Wien Atzgersdorf"};
  f[STREAM_58A_HIETZING] = {60201395, "58A", "Wien Hietzing"};
  // Others left defaulted (DIVA=0) so fetcher skips them.

  time_t now = makeLocal(2026, 5, 16, 22, 0);
  auto r = fetchSchedule(net, now, f, makeCfg());
  TEST_ASSERT_TRUE(r.ok);
  TEST_ASSERT_EQUAL_INT(1, r.calls_attempted); // dedup: one DIVA → one call
  TEST_ASSERT_EQUAL_INT(0, r.calls_failed);
  TEST_ASSERT_EQUAL_INT64(makeLocal(2026, 5, 16, 23, 50),
                          r.hint[STREAM_58A_ATZ].last_today);
  TEST_ASSERT_EQUAL_INT64(makeLocal(2026, 5, 17, 5, 6),
                          r.hint[STREAM_58A_ATZ].first_tomorrow[0]);
}

void test_fetchSchedule_dedup_shared_diva_only_calls_once() {
  FakeNet net;
  net.routes.emplace_back("name_dm=60201349", kSuedResponse);

  ScheduleStreamFilter f[STREAM_COUNT];
  // Both U1 streams share Südtiroler Platz / Hauptbahnhof DIVA.
  f[STREAM_U1_LEOPOLDAU] = {60201349, "U1", "Leopoldau"};
  f[STREAM_U1_OBERLAA] = {60201349, "U1", "Oberlaa"};

  time_t now = makeLocal(2026, 5, 16, 22, 0);
  auto r = fetchSchedule(net, now, f, makeCfg());
  TEST_ASSERT_TRUE(r.ok);
  TEST_ASSERT_EQUAL_INT_MESSAGE(1, r.calls_attempted,
                                "shared DIVA must collapse to one call");
  TEST_ASSERT_EQUAL_INT64(makeLocal(2026, 5, 17, 4, 50),
                          r.hint[STREAM_U1_LEOPOLDAU].first_tomorrow[0]);
  TEST_ASSERT_EQUAL_INT64(makeLocal(2026, 5, 17, 5, 10),
                          r.hint[STREAM_U1_OBERLAA].first_tomorrow[0]);
}

void test_fetchSchedule_all_calls_failing_marks_not_ok() {
  FakeNet net;
  net.fail_all = true;
  ScheduleStreamFilter f[STREAM_COUNT];
  f[STREAM_58A_ATZ] = {60201395, "58A", "Wien Atzgersdorf"};

  time_t now = makeLocal(2026, 5, 16, 22, 0);
  auto r = fetchSchedule(net, now, f, makeCfg());
  TEST_ASSERT_FALSE(r.ok);
  TEST_ASSERT_EQUAL_INT(1, r.calls_attempted);
  TEST_ASSERT_EQUAL_INT(1, r.calls_failed);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_buildEfaUrl_encodes_date_and_diva);
  RUN_TEST(test_computeCutoff_returns_next_local_03_00);
  RUN_TEST(test_computeCutoff_after_midnight_still_picks_next_03_00);
  RUN_TEST(test_fetchSchedule_one_diva_two_streams);
  RUN_TEST(test_fetchSchedule_dedup_shared_diva_only_calls_once);
  RUN_TEST(test_fetchSchedule_all_calls_failing_marks_not_ok);
  return UNITY_END();
}
