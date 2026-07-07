// Tier 2 — call-sequence recording for runWarmCycle. The original R1
// (subtile Verhaltensdrift im Sleep/Render-Pfad) is the bug class this
// catches: any reorder of "fetch → render → save → sleep" surfaces as a
// changed call ordering. Six variants — happy / wifi_down / ntp_fail /
// fetch_fail_prestale / fetch_fail_stale / cold_happy — cover the
// behavioural branches.

#include "logic/cycle_runner.h"
#include "recording_fakes.h"

#include <algorithm>
#include <unity.h>

using namespace bustaferl;
using namespace bustaferl::test;

namespace {

constexpr time_t kSyncedNow = 1736000000;

bool traceContains(const std::vector<std::string> &trace,
                   const std::string &needle) {
  return std::any_of(trace.begin(), trace.end(), [&](const std::string &s) {
    return s.find(needle) != std::string::npos;
  });
}

// Returns the index of the first trace entry whose body contains `needle`,
// starting at `from`. -1 if not found.
int indexOf(const std::vector<std::string> &trace, const std::string &needle,
            int from = 0) {
  for (int i = from; i < static_cast<int>(trace.size()); ++i) {
    if (trace[i].find(needle) != std::string::npos)
      return i;
  }
  return -1;
}

void assertOrdered(const std::vector<std::string> &trace,
                   const std::vector<std::string> &needles) {
  int last = -1;
  for (const auto &n : needles) {
    int idx = indexOf(trace, n, last + 1);
    if (idx < 0) {
      std::string msg = "missing in-order: " + n;
      TEST_FAIL_MESSAGE(msg.c_str());
    }
    last = idx;
  }
}

struct WarmFixture {
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

  WarmFixture(bool wifi_ok, bool http_ok, bool synced)
      : clock(trace, kSyncedNow, synced), net(trace, wifi_ok, http_ok, "{}"),
        sleep(trace, WakeCause::Timer), store(trace), display(trace),
        renderer(trace) {
    cfg.api_base = "http://api/";
    cfg.efa_base = "http://efa/";
    // Pretend we synced once a while back so the NTP-periodic-refresh branch
    // doesn't fire (separate concern from the warm-cycle path under test).
    meta.last_ntp_sync = kSyncedNow - 60;
    meta.last_api_success = kSyncedNow - 30;
    meta.framebuffer_valid = false;
    store.seedMeta(meta);
  }

  CycleDeps deps() {
    return CycleDeps{clock,    net,  sleep, store, display,
                     renderer, curr, prev,  cfg};
  }
};

} // namespace

void setUp() {}
void tearDown() {}

void test_warm_happy_renders_and_saves_before_sleep() {
  WarmFixture fx(/*wifi_ok=*/true, /*http_ok=*/true, /*synced=*/true);
  CycleDeps deps = fx.deps();
  runWarmCycle(deps, fx.meta);

  // Key invariants: WiFi connect happens, then renderer.render, then
  // store.saveFramebuffer, then store.saveMeta, then exactly one deepSleep.
  // The actual http call shape depends on retry policy and parser state —
  // we don't pin it here, the Tier-1 + parser tests own that.
  assertOrdered(fx.trace,
                {"net.connect", "renderer.render", "store.saveFramebuffer",
                 "store.saveMeta", "sleep.deepSleep"});
  TEST_ASSERT_EQUAL(1, fx.sleep.deep_sleep_calls);
  TEST_ASSERT_EQUAL(0, fx.sleep.light_sleep_calls);
  TEST_ASSERT_EQUAL(1, fx.renderer.calls);
  TEST_ASSERT_EQUAL(1, fx.store.save_meta_calls);
}

void test_warm_wifi_down_skips_render_and_deepSleeps_poll_interval() {
  WarmFixture fx(/*wifi_ok=*/false, /*http_ok=*/true, /*synced=*/true);
  fx.meta.last_api_success = kSyncedNow - 30; // not yet stale
  CycleDeps deps = fx.deps();
  runWarmCycle(deps, fx.meta);

  // No renderer call (still fresh enough to keep last frame); no httpGet.
  TEST_ASSERT_EQUAL(0, fx.renderer.calls);
  TEST_ASSERT_EQUAL(0, fx.net.http_calls);
  TEST_ASSERT_EQUAL(1, fx.sleep.deep_sleep_calls);
  TEST_ASSERT_EQUAL_UINT(fx.cfg.poll_interval_s,
                         fx.sleep.last_deep_sleep_seconds);
  TEST_ASSERT_FALSE(traceContains(fx.trace, "renderer.render"));
}

void test_warm_wifi_down_stale_renders_stale_overlay() {
  WarmFixture fx(/*wifi_ok=*/false, /*http_ok=*/true, /*synced=*/true);
  // Stale: last api success well past threshold.
  fx.meta.last_api_success = kSyncedNow - 10000;
  CycleDeps deps = fx.deps();
  runWarmCycle(deps, fx.meta);

  TEST_ASSERT_EQUAL(1, fx.renderer.calls);
  TEST_ASSERT_EQUAL(OverlayKind::Stale, fx.renderer.last_overlay);
  TEST_ASSERT_EQUAL(1, fx.sleep.deep_sleep_calls);
  TEST_ASSERT_EQUAL(0, fx.net.http_calls);
}

void test_warm_unsynced_clock_forces_ntp_then_continues() {
  WarmFixture fx(/*wifi_ok=*/true, /*http_ok=*/true, /*synced=*/false);
  CycleDeps deps = fx.deps();
  runWarmCycle(deps, fx.meta);

  // NTP forced because isSynced() == false, sync succeeded → cycle continues
  // past the unsynced guard and renders + sleeps normally.
  TEST_ASSERT_GREATER_OR_EQUAL(1, fx.clock.ntp_sync_calls);
  TEST_ASSERT_EQUAL(1, fx.renderer.calls);
  TEST_ASSERT_EQUAL(1, fx.sleep.deep_sleep_calls);
  assertOrdered(fx.trace,
                {"net.connect", "clock.isSynced -> false", "clock.ntpSync",
                 "renderer.render", "sleep.deepSleep"});
}

void test_warm_fetch_fail_prestale_keeps_last_frame() {
  // WiFi up, HTTP fails, last_api_success recent → don't redraw, just sleep.
  WarmFixture fx(/*wifi_ok=*/true, /*http_ok=*/false, /*synced=*/true);
  fx.meta.last_api_success = kSyncedNow - 30; // fresh
  CycleDeps deps = fx.deps();
  runWarmCycle(deps, fx.meta);

  TEST_ASSERT_EQUAL(0, fx.renderer.calls);
  TEST_ASSERT_EQUAL(0, fx.display.draw_partial_calls);
  TEST_ASSERT_EQUAL(0, fx.display.light_full_calls);
  TEST_ASSERT_EQUAL(1, fx.sleep.deep_sleep_calls);
  TEST_ASSERT_EQUAL(1, fx.store.save_meta_calls);
}

void test_warm_partial_fetch_rescued_within_window() {
  // OGD GETs succeed, the ÖBB POST fails → partial snapshot rendered first.
  // While the rescue paces via lightSleep, the hook advances the clock and
  // heals the POST — the rescue must fetch a complete snapshot no earlier
  // than the window start and push exactly one extra refresh.
  WarmFixture fx(/*wifi_ok=*/true, /*http_ok=*/true, /*synced=*/true);
  fx.net.setPostOk(false);
  fx.sleep.on_light_sleep = [&fx](unsigned s) {
    fx.clock.advance(static_cast<time_t>(s));
    fx.net.setPostOk(true);
  };
  CycleDeps deps = fx.deps();
  runWarmCycle(deps, fx.meta);

  TEST_ASSERT_EQUAL(2, fx.renderer.calls);
  // Rescue paced at least once (the wait into the window) before refetching.
  TEST_ASSERT_GREATER_OR_EQUAL(1, fx.sleep.light_sleep_calls);
  assertOrdered(fx.trace,
                {"renderer.render", "sleep.lightSleep", "net.httpPost",
                 "renderer.render", "store.saveMeta"});
  TEST_ASSERT_EQUAL(1, fx.store.save_meta_calls);
}

void test_warm_partial_fetch_rescue_gives_up_after_window() {
  // ÖBB POST keeps failing; the clock advances during the pacing sleeps, so
  // the rescue runs its attempts and stops at the window end — one render
  // only, then the normal sleep.
  WarmFixture fx(/*wifi_ok=*/true, /*http_ok=*/true, /*synced=*/true);
  fx.net.setPostOk(false);
  fx.sleep.on_light_sleep = [&fx](unsigned s) {
    fx.clock.advance(static_cast<time_t>(s));
  };
  CycleDeps deps = fx.deps();
  runWarmCycle(deps, fx.meta);

  TEST_ASSERT_EQUAL(1, fx.renderer.calls);
  TEST_ASSERT_EQUAL(1, fx.sleep.deep_sleep_calls);
  TEST_ASSERT_EQUAL(1, fx.store.save_meta_calls);
}

void test_warm_fetch_fail_stale_renders_stale_overlay() {
  WarmFixture fx(/*wifi_ok=*/true, /*http_ok=*/false, /*synced=*/true);
  // Stale: last api success well past threshold.
  fx.meta.last_api_success = kSyncedNow - 10000;
  CycleDeps deps = fx.deps();
  runWarmCycle(deps, fx.meta);

  TEST_ASSERT_EQUAL(1, fx.renderer.calls);
  TEST_ASSERT_EQUAL(OverlayKind::Stale, fx.renderer.last_overlay);
  TEST_ASSERT_EQUAL(1, fx.sleep.deep_sleep_calls);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_warm_happy_renders_and_saves_before_sleep);
  RUN_TEST(test_warm_wifi_down_skips_render_and_deepSleeps_poll_interval);
  RUN_TEST(test_warm_wifi_down_stale_renders_stale_overlay);
  RUN_TEST(test_warm_unsynced_clock_forces_ntp_then_continues);
  RUN_TEST(test_warm_fetch_fail_prestale_keeps_last_frame);
  RUN_TEST(test_warm_partial_fetch_rescued_within_window);
  RUN_TEST(test_warm_partial_fetch_rescue_gives_up_after_window);
  RUN_TEST(test_warm_fetch_fail_stale_renders_stale_overlay);
  return UNITY_END();
}
