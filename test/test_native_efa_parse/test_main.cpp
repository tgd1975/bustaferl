#include "config.h"
#include "data/efa_parse.h"

#include <cstdlib>
#include <ctime>
#include <string>
#include <unity.h>

using namespace bustaferl;

namespace {

// Fixture: shortened response from the live EFA endpoint (Tullnertalgasse,
// DIVA 60201395), trimmed to the fields the parser consumes. Two 58A streams
// share this DIVA — Atzgersdorf and Hietzing.
const char *kTullJson = R"JSON({
  "departureList": [
    { "dateTime": { "year":"2026","month":"5","day":"17","hour":"5","minute":"6" },
      "servingLine": { "number":"58A", "direction":"Wien Atzgersdorf" } },
    { "dateTime": { "year":"2026","month":"5","day":"17","hour":"5","minute":"15" },
      "servingLine": { "number":"58A", "direction":"Wien Hietzing" } },
    { "dateTime": { "year":"2026","month":"5","day":"17","hour":"5","minute":"30" },
      "servingLine": { "number":"58A", "direction":"Wien Atzgersdorf" } },
    { "dateTime": { "year":"2026","month":"5","day":"17","hour":"5","minute":"45" },
      "servingLine": { "number":"58A", "direction":"Wien Hietzing" } },
    { "dateTime": { "year":"2026","month":"5","day":"17","hour":"5","minute":"55" },
      "servingLine": { "number":"58A", "direction":"Wien Atzgersdorf" } }
  ]
})JSON";

// Evening fixture: several entries before midnight (today's tail) plus a few
// next-morning entries (tomorrow's first). The three pre-cutoff entries
// exercise the `next_today[2]` rolling window (keeps the latest two).
const char *kEveningJson = R"JSON({
  "departureList": [
    { "dateTime": { "year":"2026","month":"5","day":"16","hour":"22","minute":"50" },
      "servingLine": { "number":"58A", "direction":"Wien Atzgersdorf" } },
    { "dateTime": { "year":"2026","month":"5","day":"16","hour":"23","minute":"15" },
      "servingLine": { "number":"58A", "direction":"Wien Atzgersdorf" } },
    { "dateTime": { "year":"2026","month":"5","day":"16","hour":"23","minute":"45" },
      "servingLine": { "number":"58A", "direction":"Wien Atzgersdorf" } },
    { "dateTime": { "year":"2026","month":"5","day":"17","hour":"5","minute":"6" },
      "servingLine": { "number":"58A", "direction":"Wien Atzgersdorf" } },
    { "dateTime": { "year":"2026","month":"5","day":"17","hour":"5","minute":"30" },
      "servingLine": { "number":"58A", "direction":"Wien Atzgersdorf" } },
    { "dateTime": { "year":"2026","month":"5","day":"17","hour":"5","minute":"55" },
      "servingLine": { "number":"58A", "direction":"Wien Atzgersdorf" } }
  ]
})JSON";

void buildFilters(ScheduleStreamFilter (&f)[STREAM_COUNT]) {
  // Only two streams populated for these tests; others stay defaulted (DIVA=0)
  // so the parser leaves their hints alone.
  f[STREAM_58A_ATZ] = {60201395, "58A", "Wien Atzgersdorf", ""};
  f[STREAM_58A_HIETZING] = {60201395, "58A", "Wien Hietzing", ""};
}

// Build a local-time `time_t` for tests. TZ is pinned to Vienna in setUp so
// May falls into CEST (UTC+2).
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

} // namespace

void setUp() {
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();
}
void tearDown() {}

void test_parses_morning_into_first_tomorrow() {
  ScheduleStreamFilter f[STREAM_COUNT];
  buildFilters(f);
  ScheduleHint h[STREAM_COUNT]{};
  // Cutoff = 2026-05-17 03:00 local: every fixture entry is past cutoff.
  time_t cutoff = makeLocal(2026, 5, 17, 3, 0);
  TEST_ASSERT_TRUE(parseEfaResponse(kTullJson, 60201395, f, cutoff, h));

  // STREAM_58A_ATZ: 05:06 and 05:30 are the first two matches.
  TEST_ASSERT_EQUAL_INT64(makeLocal(2026, 5, 17, 5, 6),
                          h[STREAM_58A_ATZ].first_tomorrow[0]);
  TEST_ASSERT_EQUAL_INT64(makeLocal(2026, 5, 17, 5, 30),
                          h[STREAM_58A_ATZ].first_tomorrow[1]);
  TEST_ASSERT_EQUAL_INT64(0, h[STREAM_58A_ATZ].last_today);

  // STREAM_58A_HIETZING: 05:15 and 05:45.
  TEST_ASSERT_EQUAL_INT64(makeLocal(2026, 5, 17, 5, 15),
                          h[STREAM_58A_HIETZING].first_tomorrow[0]);
  TEST_ASSERT_EQUAL_INT64(makeLocal(2026, 5, 17, 5, 45),
                          h[STREAM_58A_HIETZING].first_tomorrow[1]);
}

void test_splits_at_cutoff_into_last_today_and_first_tomorrow() {
  ScheduleStreamFilter f[STREAM_COUNT];
  buildFilters(f);
  ScheduleHint h[STREAM_COUNT]{};
  // Cutoff = 2026-05-17 03:00: pre-cutoff entries (22:50, 23:15, 23:45) feed
  // last_today + next_today; 05:06 etc. above feed first_tomorrow.
  time_t cutoff = makeLocal(2026, 5, 17, 3, 0);
  TEST_ASSERT_TRUE(parseEfaResponse(kEveningJson, 60201395, f, cutoff, h));

  TEST_ASSERT_EQUAL_INT64(makeLocal(2026, 5, 16, 23, 45),
                          h[STREAM_58A_ATZ].last_today);
  TEST_ASSERT_EQUAL_INT64(makeLocal(2026, 5, 17, 5, 6),
                          h[STREAM_58A_ATZ].first_tomorrow[0]);
  TEST_ASSERT_EQUAL_INT64(makeLocal(2026, 5, 17, 5, 30),
                          h[STREAM_58A_ATZ].first_tomorrow[1]);
  // Third post-cutoff (05:55) must NOT overwrite — we keep the chronologically
  // first two.
  TEST_ASSERT_NOT_EQUAL_INT64(makeLocal(2026, 5, 17, 5, 55),
                              h[STREAM_58A_ATZ].first_tomorrow[1]);
}

void test_next_today_keeps_latest_two_pre_cutoff() {
  // Smell 13 (Schritt 2.3): evening bridge. The parser must retain the two
  // chronologically *latest* pre-cutoff entries in `next_today` so the slot
  // merger can fill the display in the late-evening dead zone.
  ScheduleStreamFilter f[STREAM_COUNT];
  buildFilters(f);
  ScheduleHint h[STREAM_COUNT]{};
  time_t cutoff = makeLocal(2026, 5, 17, 3, 0);
  TEST_ASSERT_TRUE(parseEfaResponse(kEveningJson, 60201395, f, cutoff, h));

  // Three pre-cutoff entries in fixture: 22:50, 23:15, 23:45 → keep
  // 23:15+23:45.
  TEST_ASSERT_EQUAL_INT64(makeLocal(2026, 5, 16, 23, 15),
                          h[STREAM_58A_ATZ].next_today[0]);
  TEST_ASSERT_EQUAL_INT64(makeLocal(2026, 5, 16, 23, 45),
                          h[STREAM_58A_ATZ].next_today[1]);
  // Earlier 22:50 must have been rolled out of the window.
  TEST_ASSERT_NOT_EQUAL_INT64(makeLocal(2026, 5, 16, 22, 50),
                              h[STREAM_58A_ATZ].next_today[0]);
}

void test_next_today_empty_when_no_pre_cutoff_match() {
  // All entries in kTullJson sit past the 03:00 cutoff → next_today stays 0.
  ScheduleStreamFilter f[STREAM_COUNT];
  buildFilters(f);
  ScheduleHint h[STREAM_COUNT]{};
  time_t cutoff = makeLocal(2026, 5, 17, 3, 0);
  TEST_ASSERT_TRUE(parseEfaResponse(kTullJson, 60201395, f, cutoff, h));

  TEST_ASSERT_EQUAL_INT64(0, h[STREAM_58A_ATZ].next_today[0]);
  TEST_ASSERT_EQUAL_INT64(0, h[STREAM_58A_ATZ].next_today[1]);
}

void test_unrelated_diva_leaves_other_streams_untouched() {
  ScheduleStreamFilter f[STREAM_COUNT];
  buildFilters(f);
  ScheduleHint h[STREAM_COUNT]{};
  // Seed STREAM_SBAHN_HBF with sentinel values; this call addresses DIVA
  // 60201395 (Tullnertalgasse) only, so the S-Bahn hint must survive intact.
  h[STREAM_SBAHN_HBF].last_today = 99999;
  h[STREAM_SBAHN_HBF].first_tomorrow[0] = 88888;

  TEST_ASSERT_TRUE(parseEfaResponse(kTullJson, 60201395, f,
                                    makeLocal(2026, 5, 17, 3, 0), h));
  TEST_ASSERT_EQUAL_INT64(99999, h[STREAM_SBAHN_HBF].last_today);
  TEST_ASSERT_EQUAL_INT64(88888, h[STREAM_SBAHN_HBF].first_tomorrow[0]);
}

void test_malformed_json_returns_false() {
  ScheduleStreamFilter f[STREAM_COUNT];
  buildFilters(f);
  ScheduleHint h[STREAM_COUNT]{};
  TEST_ASSERT_FALSE(parseEfaResponse("{ not valid", 60201395, f, 0, h));
}

void test_empty_departure_list_is_ok() {
  ScheduleStreamFilter f[STREAM_COUNT];
  buildFilters(f);
  ScheduleHint h[STREAM_COUNT]{};
  TEST_ASSERT_TRUE(parseEfaResponse(R"({"departureList":[]})", 60201395, f,
                                    makeLocal(2026, 5, 17, 3, 0), h));
  TEST_ASSERT_EQUAL_INT64(0, h[STREAM_58A_ATZ].first_tomorrow[0]);
}

void test_direction_prefix_filter_excludes_other_directions() {
  ScheduleStreamFilter f[STREAM_COUNT];
  buildFilters(f);
  // Tighten: only Atzgersdorf, Hietzing left unfiltered (still configured).
  ScheduleHint h[STREAM_COUNT]{};
  TEST_ASSERT_TRUE(parseEfaResponse(kTullJson, 60201395, f,
                                    makeLocal(2026, 5, 17, 3, 0), h));
  // Atzgersdorf got 3 entries (05:06, 05:30, 05:55) → first two retained.
  TEST_ASSERT_EQUAL_INT64(makeLocal(2026, 5, 17, 5, 6),
                          h[STREAM_58A_ATZ].first_tomorrow[0]);
  TEST_ASSERT_EQUAL_INT64(makeLocal(2026, 5, 17, 5, 30),
                          h[STREAM_58A_ATZ].first_tomorrow[1]);
  // Hietzing got 2 entries (05:15, 05:45) → both retained.
  TEST_ASSERT_EQUAL_INT64(makeLocal(2026, 5, 17, 5, 15),
                          h[STREAM_58A_HIETZING].first_tomorrow[0]);
  TEST_ASSERT_EQUAL_INT64(makeLocal(2026, 5, 17, 5, 45),
                          h[STREAM_58A_HIETZING].first_tomorrow[1]);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_parses_morning_into_first_tomorrow);
  RUN_TEST(test_splits_at_cutoff_into_last_today_and_first_tomorrow);
  RUN_TEST(test_next_today_keeps_latest_two_pre_cutoff);
  RUN_TEST(test_next_today_empty_when_no_pre_cutoff_match);
  RUN_TEST(test_unrelated_diva_leaves_other_streams_untouched);
  RUN_TEST(test_malformed_json_returns_false);
  RUN_TEST(test_empty_departure_list_is_ok);
  RUN_TEST(test_direction_prefix_filter_excludes_other_directions);
  return UNITY_END();
}
