#include <unity.h>

#include "logic/sleep_planner.h"

using namespace bustaferl;

static SleepConfig cfg{900, 30, 120, 1800};

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

void test_no_departures_uses_no_data_sleep() {
  StreamSnapshot snap;
  auto d = planSleep(snap, 1000, cfg);
  TEST_ASSERT_EQUAL(static_cast<int>(Mode::DeepSleep),
                    static_cast<int>(d.mode));
  TEST_ASSERT_EQUAL_UINT(1800, d.seconds);
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

void setUp() {}
void tearDown() {}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_far_future_deep_sleeps);
  RUN_TEST(test_within_active_threshold_stays_awake);
  RUN_TEST(test_wake_point_in_past_stays_awake);
  RUN_TEST(test_no_departures_uses_no_data_sleep);
  RUN_TEST(test_uses_minimum_across_streams);
  return UNITY_END();
}
