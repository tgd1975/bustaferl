// Tier 1 for cycle_runner: small, deterministic table-test for the
// nightly-clean promotion decision. The big behavioural sweep — call-
// sequence recording for runColdCycle/runWarmCycle — lives in 7.3
// (test_native_cycle_runner_*). This file pins the one extracted
// public helper, plus a smoke test of the CycleConfig defaults.

#include "logic/cycle_runner.h"

#include <unity.h>

using namespace bustaferl;

namespace {

CycleConfig defaultCfg() {
  CycleConfig c;
  c.long_sleep_for_nightly_clean_s = 4 * 3600;
  c.nightly_deep_clean_interval_s = 20 * 3600;
  return c;
}

constexpr time_t kNow = 1736000000;

} // namespace

void setUp() {}
void tearDown() {}

void test_short_sleep_never_promotes() {
  CycleConfig cfg = defaultCfg();
  // Last clean infinitely old, but sleep below the long-sleep threshold.
  bool promote = shouldPromoteToNightlyClean(
      /*next_sleep_s=*/1800, kNow, /*last_deep_clean=*/0, cfg);
  TEST_ASSERT_FALSE(promote);
}

void test_at_long_sleep_threshold_does_not_promote() {
  // Boundary: the comparison is strictly greater, so equality fails.
  CycleConfig cfg = defaultCfg();
  bool promote = shouldPromoteToNightlyClean(
      /*next_sleep_s=*/cfg.long_sleep_for_nightly_clean_s, kNow,
      /*last_deep_clean=*/0, cfg);
  TEST_ASSERT_FALSE(promote);
}

void test_long_sleep_with_old_clean_promotes() {
  CycleConfig cfg = defaultCfg();
  time_t old_clean = kNow - cfg.nightly_deep_clean_interval_s - 60;
  bool promote = shouldPromoteToNightlyClean(
      /*next_sleep_s=*/cfg.long_sleep_for_nightly_clean_s + 1, kNow, old_clean,
      cfg);
  TEST_ASSERT_TRUE(promote);
}

void test_long_sleep_with_recent_clean_does_not_promote() {
  CycleConfig cfg = defaultCfg();
  time_t recent_clean = kNow - 60; // a minute ago
  bool promote = shouldPromoteToNightlyClean(
      /*next_sleep_s=*/cfg.long_sleep_for_nightly_clean_s + 1, kNow,
      recent_clean, cfg);
  TEST_ASSERT_FALSE(promote);
}

void test_long_sleep_never_cleaned_promotes() {
  CycleConfig cfg = defaultCfg();
  bool promote = shouldPromoteToNightlyClean(
      /*next_sleep_s=*/cfg.long_sleep_for_nightly_clean_s + 1, kNow,
      /*last_deep_clean=*/0, cfg);
  TEST_ASSERT_TRUE(promote);
}

void test_config_defaults_match_legacy_constants() {
  // Guard the values that used to live as magic numbers in main.cpp. If the
  // production cycle drifts, the legacy long-term tests' baselines drift
  // alongside it — this is the explicit pin.
  CycleConfig c;
  TEST_ASSERT_EQUAL_UINT(4u * 3600u, c.long_sleep_for_nightly_clean_s);
  TEST_ASSERT_EQUAL_INT(20 * 3600, c.nightly_deep_clean_interval_s);
  TEST_ASSERT_EQUAL_INT(180, c.stale_threshold_s);
  TEST_ASSERT_EQUAL_UINT(30u, c.poll_interval_s);
  TEST_ASSERT_EQUAL_INT(900, c.wake_before_bus_s);
  TEST_ASSERT_EQUAL_INT(3, c.filter_health_dead_after);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_short_sleep_never_promotes);
  RUN_TEST(test_at_long_sleep_threshold_does_not_promote);
  RUN_TEST(test_long_sleep_with_old_clean_promotes);
  RUN_TEST(test_long_sleep_with_recent_clean_does_not_promote);
  RUN_TEST(test_long_sleep_never_cleaned_promotes);
  RUN_TEST(test_config_defaults_match_legacy_constants);
  return UNITY_END();
}
