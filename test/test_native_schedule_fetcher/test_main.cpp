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
  HttpResult httpGet(const std::string &url, std::string &out) override {
    urls_seen.push_back(url);
    if (fail_all)
      return {false, 0};
    for (const auto &r : routes) {
      if (url.find(r.first) != std::string::npos) {
        out = r.second;
        return {true, 200};
      }
    }
    return {false, 0};
  }
  HttpResult httpPost(const std::string &, const std::string &,
                      const std::string &, std::string &) override {
    return {false, 0};
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

void test_computeCutoff_small_hours_picks_today_03_00() {
  // 02:30 local on 17 May → cutoff = 03:00 local the SAME day (30 min ahead),
  // not 18 May. The next cutoff at or after `now` is today's, since 03:00 has
  // not passed yet. (Anchoring a day too far ahead here was the bug that hid
  // this morning's ~05:00 departures under "first_tomorrow".)
  time_t now = makeLocal(2026, 5, 17, 2, 30);
  time_t cut = computeCutoff(now, 3);
  TEST_ASSERT_EQUAL_INT64(makeLocal(2026, 5, 17, 3, 0), cut);
}

void test_computeCutoff_after_cutoff_picks_tomorrow() {
  // 06:00 local on 17 May → today's 03:00 already passed → cutoff = 18 May
  // 03:00.
  time_t now = makeLocal(2026, 5, 17, 6, 0);
  time_t cut = computeCutoff(now, 3);
  TEST_ASSERT_EQUAL_INT64(makeLocal(2026, 5, 18, 3, 0), cut);
}

// The reported field bug: a schedule refresh in the small hours (00:07) must
// anchor the EFA query at `now`, not at a future 22:00, and split
// today/tomorrow at TODAY's 03:00 — so this morning's ~05:00 departures land in
// first_tomorrow and tonight's tail (00:28) in next_today, instead of
// everything shifting a day.
void test_fetchSchedule_small_hours_anchors_at_now() {
  FakeNet net;
  // EFA fixture as if queried from 00:07: tonight's tail + this morning.
  const char *kNightResponse = R"JSON({
    "departureList": [
      { "dateTime": { "year":"2026","month":"7","day":"14","hour":"0","minute":"28" },
        "servingLine": { "number":"58A", "direction":"Wien Atzgersdorf" } },
      { "dateTime": { "year":"2026","month":"7","day":"14","hour":"5","minute":"6" },
        "servingLine": { "number":"58A", "direction":"Wien Atzgersdorf" } },
      { "dateTime": { "year":"2026","month":"7","day":"14","hour":"5","minute":"30" },
        "servingLine": { "number":"58A", "direction":"Wien Atzgersdorf" } }
    ]
  })JSON";
  net.routes.emplace_back("name_dm=60201395", kNightResponse);

  ScheduleStreamFilter f[STREAM_COUNT];
  f[STREAM_58A_ATZ] = {60201395, "58A", "Wien Atzgersdorf", ""};

  time_t now = makeLocal(2026, 7, 14, 0, 7);
  auto r = fetchSchedule(net, now, f, makeCfg());
  TEST_ASSERT_TRUE(r.ok);

  // Query anchored at now (00:07), not the future 22:00.
  bool anchored_now = false;
  for (const auto &u : net.urls_seen) {
    if (u.find("itdTimeHour=00") != std::string::npos &&
        u.find("itdDateDay=14") != std::string::npos) {
      anchored_now = true;
    }
  }
  TEST_ASSERT_TRUE(anchored_now);

  // 00:28 is before today's 03:00 cutoff → tonight's tail.
  TEST_ASSERT_EQUAL_INT64(makeLocal(2026, 7, 14, 0, 28),
                          r.hint[STREAM_58A_ATZ].next_today[1]);
  // 05:06 is after today's 03:00 cutoff → this morning's first bus.
  TEST_ASSERT_EQUAL_INT64(makeLocal(2026, 7, 14, 5, 6),
                          r.hint[STREAM_58A_ATZ].first_tomorrow[0]);
}

void test_fetchSchedule_one_diva_two_streams() {
  FakeNet net;
  net.routes.emplace_back("name_dm=60201395", kTullResponse);

  ScheduleStreamFilter f[STREAM_COUNT];
  f[STREAM_58A_ATZ] = {60201395, "58A", "Wien Atzgersdorf", ""};
  f[STREAM_58A_HIETZING] = {60201395, "58A", "Wien Hietzing", ""};
  // Others left defaulted (DIVA=0) so fetcher skips them.

  time_t now = makeLocal(2026, 5, 16, 22, 0);
  auto r = fetchSchedule(net, now, f, makeCfg());
  TEST_ASSERT_TRUE(r.ok);
  TEST_ASSERT_EQUAL_INT(1, r.calls_attempted); // dedup: one DIVA → one call
  TEST_ASSERT_EQUAL_INT(0, r.calls_failed);
  TEST_ASSERT_EQUAL_INT64(makeLocal(2026, 5, 16, 23, 50),
                          r.hint[STREAM_58A_ATZ].last_today);
  TEST_ASSERT_EQUAL_INT64(makeLocal(2026, 5, 16, 23, 50),
                          r.hint[STREAM_58A_ATZ].next_today[1]);
  TEST_ASSERT_EQUAL_INT64(makeLocal(2026, 5, 17, 5, 6),
                          r.hint[STREAM_58A_ATZ].first_tomorrow[0]);
}

// Removed in v2 Session B: this exercised U1-stream-DIVA-dedup, but v2 has
// no two EFA streams sharing a DIVA. Schritt 8.4 (Session F) will rewrite
// the bucket with a v2-appropriate scenario; for now the dedup mechanism is
// covered by test_fetchSchedule_one_diva_two_streams.

void test_fetchSchedule_all_calls_failing_marks_not_ok() {
  FakeNet net;
  net.fail_all = true;
  ScheduleStreamFilter f[STREAM_COUNT];
  f[STREAM_58A_ATZ] = {60201395, "58A", "Wien Atzgersdorf", ""};

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
  RUN_TEST(test_computeCutoff_small_hours_picks_today_03_00);
  RUN_TEST(test_computeCutoff_after_cutoff_picks_tomorrow);
  RUN_TEST(test_fetchSchedule_one_diva_two_streams);
  RUN_TEST(test_fetchSchedule_small_hours_anchors_at_now);
  RUN_TEST(test_fetchSchedule_all_calls_failing_marks_not_ok);
  return UNITY_END();
}
