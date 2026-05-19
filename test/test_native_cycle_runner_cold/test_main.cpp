// Tier 2 — call-sequence recording for runColdCycle. Cold path is rarer in
// practice but the most dangerous failure mode: a bad cold cycle leaves the
// device in a guru-meditation loop without a panel update. Three variants:
// happy (Ok), retry_later, give_up.

#include "../test_native_cycle_runner_warm/recording_fakes.h"
#include "logic/cycle_runner.h"

#include <unity.h>

using namespace bustaferl;
using namespace bustaferl::test;

namespace {

constexpr time_t kSyncedNow = 1736000000;

struct ColdFixture {
  std::vector<std::string> trace;
  RecordingClock clock;
  RecordingNet net;
  RecordingSleep sleep;
  RecordingStore store;
  RecordingDisplay display;
  RecordingRenderer renderer;
  Frame curr;
  Frame prev;
  CycleConfig cfg;
  PersistedMeta meta;

  ColdFixture(bool wifi_ok, bool http_ok, uint8_t retries_so_far)
      : clock(trace, kSyncedNow, /*synced=*/wifi_ok),
        net(trace, wifi_ok, http_ok, "{}"), sleep(trace, WakeCause::ColdBoot),
        store(trace), display(trace), renderer(trace) {
    cfg.api_base = "http://api/";
    cfg.efa_base = "http://efa/";
    meta.cold_boot_retries = retries_so_far;
  }

  CycleDeps deps() {
    return CycleDeps{clock,    net,  sleep, store, display,
                     renderer, curr, prev,  cfg};
  }
};

} // namespace

void setUp() {}
void tearDown() {}

void test_cold_happy_deep_cleans_and_renders_once() {
  ColdFixture fx(/*wifi_ok=*/true, /*http_ok=*/true, /*retries=*/0);
  CycleDeps deps = fx.deps();
  runColdCycle(deps, fx.meta);

  // Cold path always ends with a deepClean (known-good panel) and exactly
  // one renderer.render call followed by a deepSleep.
  TEST_ASSERT_EQUAL(1, fx.renderer.calls);
  TEST_ASSERT_EQUAL(1, fx.display.deep_clean_calls);
  TEST_ASSERT_EQUAL(0, fx.display.draw_partial_calls);
  TEST_ASSERT_EQUAL(1, fx.sleep.deep_sleep_calls);
  TEST_ASSERT_TRUE(fx.meta.framebuffer_valid);
  // last_deep_clean stamped.
  TEST_ASSERT_EQUAL_INT64(kSyncedNow, fx.meta.last_deep_clean);
  // cold_boot_retries reset after success.
  TEST_ASSERT_EQUAL(0, fx.meta.cold_boot_retries);
}

void test_cold_retry_later_increments_retries_and_short_sleeps() {
  ColdFixture fx(/*wifi_ok=*/false, /*http_ok=*/true, /*retries=*/0);
  CycleDeps deps = fx.deps();
  runColdCycle(deps, fx.meta);

  // No render, no display work: the cycle aborted after the failed boot.
  TEST_ASSERT_EQUAL(0, fx.renderer.calls);
  TEST_ASSERT_EQUAL(0, fx.display.deep_clean_calls);
  // Counter advanced.
  TEST_ASSERT_EQUAL(1, fx.meta.cold_boot_retries);
  TEST_ASSERT_GREATER_OR_EQUAL(1, fx.sleep.deep_sleep_calls);
  TEST_ASSERT_EQUAL_UINT(fx.cfg.cold_boot_retry_s,
                         fx.sleep.last_deep_sleep_seconds);
}

void test_cold_give_up_overlay_and_reset() {
  // After max retries, the boot sequencer returns GiveUp. Cycle renders
  // a StartFailed overlay, deep-cleans the panel, and zeroes the counter.
  ColdFixture fx(/*wifi_ok=*/false, /*http_ok=*/true,
                 /*retries=*/DEFAULT_COLD_BOOT_MAX_RETRIES);
  CycleDeps deps = fx.deps();
  runColdCycle(deps, fx.meta);

  TEST_ASSERT_EQUAL(1, fx.renderer.calls);
  TEST_ASSERT_EQUAL(OverlayKind::StartFailed, fx.renderer.last_overlay);
  TEST_ASSERT_EQUAL(1, fx.display.deep_clean_calls);
  TEST_ASSERT_EQUAL(0, fx.meta.cold_boot_retries);
  TEST_ASSERT_FALSE(fx.meta.framebuffer_valid);
  TEST_ASSERT_EQUAL_UINT(fx.cfg.cold_boot_giveup_sleep_s,
                         fx.sleep.last_deep_sleep_seconds);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_cold_happy_deep_cleans_and_renders_once);
  RUN_TEST(test_cold_retry_later_increments_retries_and_short_sleeps);
  RUN_TEST(test_cold_give_up_overlay_and_reset);
  return UNITY_END();
}
