// Tier 1 — rescue window arithmetic (logic/rescue_policy). The cycle-level
// behaviour (extra render, pacing, give-up) lives in
// test_native_cycle_runner_warm; this bucket pins the pure step decision.

#include "logic/rescue_policy.h"

#include <unity.h>

using namespace bustaferl;

namespace {

constexpr time_t kAnchor = 1736000000;

RescueConfig defaultCfg() { return RescueConfig{}; }

} // namespace

void setUp() {}
void tearDown() {}

void test_before_window_waits_until_window_start() {
  unsigned wait_s = 99;
  RescueStep s = nextRescueStep(kAnchor, kAnchor, defaultCfg(), wait_s);
  TEST_ASSERT_EQUAL(RescueStep::Wait, s);
  TEST_ASSERT_EQUAL_UINT(DEFAULT_RESCUE_WINDOW_START_S, wait_s);
}

void test_mid_wait_gets_remaining_time() {
  unsigned wait_s = 0;
  RescueStep s = nextRescueStep(kAnchor + 12, kAnchor, defaultCfg(), wait_s);
  TEST_ASSERT_EQUAL(RescueStep::Wait, s);
  TEST_ASSERT_EQUAL_UINT(DEFAULT_RESCUE_WINDOW_START_S - 12, wait_s);
}

void test_window_start_boundary_retries() {
  unsigned wait_s = 99;
  RescueStep s = nextRescueStep(kAnchor + DEFAULT_RESCUE_WINDOW_START_S,
                                kAnchor, defaultCfg(), wait_s);
  TEST_ASSERT_EQUAL(RescueStep::Retry, s);
  TEST_ASSERT_EQUAL_UINT(0, wait_s);
}

void test_window_end_boundary_still_retries() {
  unsigned wait_s = 99;
  RescueStep s = nextRescueStep(kAnchor + DEFAULT_RESCUE_WINDOW_END_S, kAnchor,
                                defaultCfg(), wait_s);
  TEST_ASSERT_EQUAL(RescueStep::Retry, s);
}

void test_past_window_stops() {
  unsigned wait_s = 99;
  RescueStep s = nextRescueStep(kAnchor + DEFAULT_RESCUE_WINDOW_END_S + 1,
                                kAnchor, defaultCfg(), wait_s);
  TEST_ASSERT_EQUAL(RescueStep::Stop, s);
  TEST_ASSERT_EQUAL_UINT(0, wait_s);
}

void test_clock_before_anchor_stops() {
  // Clock jumped backwards (NTP correction mid-cycle): give up rather than
  // sleeping on a bogus delta.
  unsigned wait_s = 99;
  RescueStep s = nextRescueStep(kAnchor - 5, kAnchor, defaultCfg(), wait_s);
  TEST_ASSERT_EQUAL(RescueStep::Stop, s);
  TEST_ASSERT_EQUAL_UINT(0, wait_s);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_before_window_waits_until_window_start);
  RUN_TEST(test_mid_wait_gets_remaining_time);
  RUN_TEST(test_window_start_boundary_retries);
  RUN_TEST(test_window_end_boundary_still_retries);
  RUN_TEST(test_past_window_stops);
  RUN_TEST(test_clock_before_anchor_stops);
  return UNITY_END();
}
