#include <unity.h>

#include "logic/sleep_planner.h"

using namespace bustaferl;

static SleepConfig cfg{900, 30, 120, 1800, 60};

static StreamSnapshot makeSnap(time_t first_departure) {
  StreamSnapshot s;
  s.api_ok = true;
  s.stream[STREAM_58A_ATZ].slot[0] = {first_departure, true, true};
  return s;
}

void test_far_future_deep_sleeps() {
  auto snap = makeSnap(10000);
  time_t now = 0;
  auto d = planSleep(snap, now, cfg);
  TEST_ASSERT_EQUAL(static_cast<int>(Mode::DeepSleep),
                    static_cast<int>(d.mode));
  // 10000 - 900 - 30 - 0 = 9070
  TEST_ASSERT_EQUAL_UINT(9070, d.seconds);
}

void test_within_active_threshold_stays_awake() {
  // departure in 950s → wake_at = 950 - 930 = 20s → < ACTIVE_THRESHOLD
  auto snap = makeSnap(950);
  auto d = planSleep(snap, 0, cfg);
  TEST_ASSERT_EQUAL(static_cast<int>(Mode::Active), static_cast<int>(d.mode));
}

void test_wake_point_in_past_stays_awake() {
  // departure in 500s → wake_at would have been at -430s
  auto snap = makeSnap(500);
  auto d = planSleep(snap, 0, cfg);
  TEST_ASSERT_EQUAL(static_cast<int>(Mode::Active), static_cast<int>(d.mode));
}

void test_api_ok_no_departures_uses_no_data_sleep() {
  // "API responded but the schedule is empty" — overnight case → long sleep.
  StreamSnapshot snap;
  snap.api_ok = true;
  auto d = planSleep(snap, 1000, cfg);
  TEST_ASSERT_EQUAL(static_cast<int>(Mode::DeepSleep),
                    static_cast<int>(d.mode));
  TEST_ASSERT_EQUAL_UINT(1800, d.seconds);
}

void test_api_failure_uses_short_retry() {
  // !snap.api_ok → upstream/network blip, retry on the api_failure cadence
  // instead of falling through to the overnight no_data_sleep.
  StreamSnapshot snap; // api_ok defaults to false
  auto d = planSleep(snap, 1000, cfg);
  TEST_ASSERT_EQUAL(static_cast<int>(Mode::DeepSleep),
                    static_cast<int>(d.mode));
  TEST_ASSERT_EQUAL_UINT(60, d.seconds);
}

void test_uses_minimum_across_streams() {
  StreamSnapshot snap;
  snap.stream[STREAM_58A_ATZ].slot[0] = {5000, true, true};
  snap.stream[STREAM_58A_HIETZING].slot[0] = {3000, true, true};
  snap.stream[STREAM_58B_ATZ].slot[1] = {2500, true, true};
  auto d = planSleep(snap, 0, cfg);
  // earliest = 2500 → wake_at = 2500 - 930 = 1570
  TEST_ASSERT_EQUAL_UINT(1570, d.seconds);
}

void test_nightly_never_cleaned_is_due() {
  TEST_ASSERT_TRUE(needsNightlyDeepClean(1700000000, 0, 20 * 3600));
}

void test_nightly_just_cleaned_is_not_due() {
  TEST_ASSERT_FALSE(needsNightlyDeepClean(1700000000, 1700000000 - 60,
                                          20 * 3600));
}

void test_nightly_at_threshold_is_due() {
  TEST_ASSERT_TRUE(needsNightlyDeepClean(1700000000, 1700000000 - 20 * 3600,
                                         20 * 3600));
}

void test_nightly_just_under_threshold_is_not_due() {
  TEST_ASSERT_FALSE(needsNightlyDeepClean(1700000000,
                                          1700000000 - (20 * 3600 - 1),
                                          20 * 3600));
}

void test_nightly_clock_behind_last_is_not_due() {
  // Defends against `now` not yet NTP-synced after deep sleep.
  TEST_ASSERT_FALSE(needsNightlyDeepClean(100, 1700000000, 20 * 3600));
}

void setUp() {}
void tearDown() {}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_far_future_deep_sleeps);
  RUN_TEST(test_within_active_threshold_stays_awake);
  RUN_TEST(test_wake_point_in_past_stays_awake);
  RUN_TEST(test_api_ok_no_departures_uses_no_data_sleep);
  RUN_TEST(test_api_failure_uses_short_retry);
  RUN_TEST(test_uses_minimum_across_streams);
  RUN_TEST(test_nightly_never_cleaned_is_due);
  RUN_TEST(test_nightly_just_cleaned_is_not_due);
  RUN_TEST(test_nightly_at_threshold_is_due);
  RUN_TEST(test_nightly_just_under_threshold_is_not_due);
  RUN_TEST(test_nightly_clock_behind_last_is_not_due);
  return UNITY_END();
}
