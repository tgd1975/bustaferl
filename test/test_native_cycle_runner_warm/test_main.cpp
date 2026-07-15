// Tier 2 — call-sequence recording for runWarmCycle. The original R1
// (subtile Verhaltensdrift im Sleep/Render-Pfad) is the bug class this
// catches: any reorder of "fetch → render → save → sleep" surfaces as a
// changed call ordering. Six variants — happy / wifi_down / ntp_fail /
// fetch_fail_prestale / fetch_fail_stale / cold_happy — cover the
// behavioural branches.

#include "logic/cycle_runner.h"
#include "logic/cycle_trace.h"
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

  CycleDeps deps(bool deep_wake = false) {
    return CycleDeps{clock,    net,  sleep, store, display,
                     renderer, curr, prev,  cfg,   deep_wake};
  }
};

// Same as WarmFixture but with a HeuristicClock whose isSynced() is derived
// from now() (real-hardware behaviour). Used for the clock-drift ("58B coma")
// scenarios where the wall clock has jumped forward but still reads > 2023.
struct HeuristicWarmFixture {
  std::vector<std::string> trace;
  HeuristicClock clock;
  RecordingNet net;
  RecordingSleep sleep;
  RecordingStore store;
  RecordingDisplay display;
  RecordingRenderer renderer;
  Frame curr;
  Frame prev;
  CycleConfig cfg;
  PersistedMeta meta;

  // now_reads: what the (possibly corrupt) RTC reports on this wake.
  // expected_wake: what the previous cycle persisted as its wake target —
  //   the reference the drift guard checks now() against.
  // true_now: where a real ntpSync() would land the clock.
  HeuristicWarmFixture(time_t now_reads, time_t expected_wake, time_t true_now)
      : clock(trace, now_reads),
        net(trace, /*wifi_ok=*/true, /*http_ok=*/true, "{}"),
        sleep(trace, WakeCause::Timer), store(trace), display(trace),
        renderer(trace) {
    cfg.api_base = "http://api/";
    cfg.efa_base = "http://efa/";
    clock.setSyncTarget(true_now);
    meta.expected_wake_at = expected_wake;
    // A prior good sync exists (so the periodic-refresh branch is a separate
    // concern); keep it just behind the expected wake.
    meta.last_ntp_sync = expected_wake;
    meta.last_api_success = expected_wake;
    meta.last_success_at = expected_wake;
    meta.has_any_data = true;
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

void test_warm_happy_stamps_cycle_trace() {
  WarmFixture fx(/*wifi_ok=*/true, /*http_ok=*/true, /*synced=*/true);
  CycleDeps deps = fx.deps();
  runWarmCycle(deps, fx.meta); // trigger defaults to Timer

  TEST_ASSERT_GREATER_OR_EQUAL(1, fx.store.save_trace_calls);
  const CycleTrace &t = fx.store.recordedTrace();
  TEST_ASSERT_EQUAL(1, t.cycle_count);
  const CycleRecord *rec = traceCycleAt(t, 0);
  TEST_ASSERT_NOT_NULL(rec);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(CycleTrigger::Timer), rec->trigger);
  TEST_ASSERT_TRUE((rec->flags & CYC_RENDERED) != 0);
}

void test_warm_button_trigger_recorded() {
  WarmFixture fx(/*wifi_ok=*/true, /*http_ok=*/true, /*synced=*/true);
  CycleDeps deps = fx.deps();
  runWarmCycle(deps, fx.meta, CycleTrigger::Button);

  const CycleRecord *rec = traceCycleAt(fx.store.recordedTrace(), 0);
  TEST_ASSERT_NOT_NULL(rec);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(CycleTrigger::Button), rec->trigger);
}

void test_warm_wifi_down_records_anomaly() {
  WarmFixture fx(/*wifi_ok=*/false, /*http_ok=*/true, /*synced=*/true);
  fx.meta.last_api_success = kSyncedNow - 30; // not yet stale
  CycleDeps deps = fx.deps();
  runWarmCycle(deps, fx.meta);

  const CycleTrace &t = fx.store.recordedTrace();
  const ErrorRecord *e = traceErrorAt(t, 0);
  TEST_ASSERT_NOT_NULL(e);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(TraceError::WifiFail), e->code);
  // The wifi-down cycle is still logged in the cycle ring.
  TEST_ASSERT_EQUAL(1, t.cycle_count);
}

void test_warm_wifi_down_old_data_renders_offline() {
  WarmFixture fx(/*wifi_ok=*/false, /*http_ok=*/true, /*synced=*/true);
  // Success past OFFLINE_THRESHOLD_S with wifi down → the selector picks
  // Offline (the "KEIN EMPFANG" screen).
  fx.meta.last_api_success = kSyncedNow - 10000;
  fx.meta.last_success_at = kSyncedNow - 10000;
  fx.meta.has_any_data = true;
  CycleDeps deps = fx.deps();
  runWarmCycle(deps, fx.meta);

  TEST_ASSERT_EQUAL(1, fx.renderer.calls);
  TEST_ASSERT_EQUAL(DisplayState::Offline, fx.renderer.last_state);
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
  // WiFi up, HTTP fails, last success recent → don't redraw, just sleep.
  // v2 selector reads `last_success_at`; legacy field also kept fresh so
  // the legacy `isStale` check (still used elsewhere) stays consistent.
  WarmFixture fx(/*wifi_ok=*/true, /*http_ok=*/false, /*synced=*/true);
  fx.meta.last_api_success = kSyncedNow - 30; // fresh
  fx.meta.last_success_at = kSyncedNow - 30;
  fx.meta.has_any_data = true;
  CycleDeps deps = fx.deps();
  runWarmCycle(deps, fx.meta);

  TEST_ASSERT_EQUAL(0, fx.renderer.calls);
  TEST_ASSERT_EQUAL(0, fx.display.draw_partial_calls);
  TEST_ASSERT_EQUAL(0, fx.display.light_full_calls);
  TEST_ASSERT_EQUAL(1, fx.sleep.deep_sleep_calls);
  TEST_ASSERT_EQUAL(1, fx.store.save_meta_calls);
}

void test_warm_fetch_fail_old_data_keeps_last_frame() {
  // WiFi up, HTTP fails, data old. With the Stale screen gone, the selector
  // returns Normal on a fetch failure regardless of data age, and the redraw
  // guard keeps the last good frame rather than blanking to "??:??". (The old
  // behavior flashed a dedicated Stale screen; now the board just holds.)
  WarmFixture fx(/*wifi_ok=*/true, /*http_ok=*/false, /*synced=*/true);
  fx.meta.last_api_success = kSyncedNow - 10000;
  fx.meta.last_success_at = kSyncedNow - 10000;
  fx.meta.has_any_data = true;
  CycleDeps deps = fx.deps();
  runWarmCycle(deps, fx.meta);

  TEST_ASSERT_EQUAL(0, fx.renderer.calls);
  TEST_ASSERT_EQUAL(0, fx.display.draw_partial_calls);
  TEST_ASSERT_EQUAL(0, fx.display.light_full_calls);
  TEST_ASSERT_EQUAL(1, fx.sleep.deep_sleep_calls);
}

// --- Clock-drift ("58B coma") regression --------------------------------
//
// Field bug: on the 58B, a warm cycle rendered departures a few hours in the
// future with nonsensical minutes. Root cause: after a deep-sleep wake the
// RTC wall clock came back CORRUPT — hours ahead of true time — but still
// past 2023, so isSynced() (lower-bound-only) reported "synced" and no
// re-sync fired. The schedule-hint merge then filtered every hint against a
// now() that was hours off, so the panel showed the wrong times.

void test_warm_clock_jumped_hours_ahead_forces_resync() {
  // The previous cycle asked to wake at kSyncedNow, but the RTC came back 3 h
  // past that — physically impossible, so it must be treated as corrupt and
  // re-synced despite reading > 2023.
  const time_t expected_wake = kSyncedNow;
  const time_t true_now = kSyncedNow + 30; // where a real sync would land
  const time_t corrupt_now = kSyncedNow + 3 * 3600;
  HeuristicWarmFixture fx(corrupt_now, expected_wake, true_now);
  CycleDeps deps = fx.deps();
  runWarmCycle(deps, fx.meta);

  // The guard must have forced an NTP re-sync despite isSynced()==true...
  TEST_ASSERT_GREATER_OR_EQUAL(1, fx.clock.ntp_sync_calls);
  // ...and the cycle then proceeds on the corrected clock, not the bogus one.
  TEST_ASSERT_EQUAL_INT64(true_now, fx.clock.now());
  // A render still happens (we didn't just bail); it uses the good time.
  TEST_ASSERT_GREATER_OR_EQUAL(1, fx.renderer.calls);
}

void test_warm_clock_lands_on_wake_target_does_not_resync() {
  // Legitimate wake: the RTC lands right on the expected-wake epoch (true for
  // any sleep length — this is what a healthy clock does). A small overshoot
  // within the jitter budget must NOT trip the guard, or we'd burn NTP every
  // wake.
  const time_t expected_wake = kSyncedNow;
  const time_t now_reads =
      kSyncedNow + 20; // 20 s wake latency, well under 30 min
  HeuristicWarmFixture fx(now_reads, expected_wake, now_reads);
  CycleDeps deps = fx.deps();
  runWarmCycle(deps, fx.meta);

  TEST_ASSERT_EQUAL(0, fx.clock.ntp_sync_calls);
  TEST_ASSERT_GREATER_OR_EQUAL(1, fx.renderer.calls);
}

void test_warm_first_boot_no_wake_reference_does_not_resync() {
  // expected_wake_at == 0 (never slept with a known clock): the guard has no
  // reference and must abstain, even though now() is arbitrarily large.
  const time_t now_reads = kSyncedNow + 10 * 3600;
  HeuristicWarmFixture fx(now_reads, /*expected_wake=*/kSyncedNow, now_reads);
  fx.meta.expected_wake_at = 0; // override: simulate first-ever warm cycle
  fx.store.seedMeta(fx.meta);
  CycleDeps deps = fx.deps();
  runWarmCycle(deps, fx.meta);

  TEST_ASSERT_EQUAL(0, fx.clock.ntp_sync_calls);
  TEST_ASSERT_GREATER_OR_EQUAL(1, fx.renderer.calls);
}

void test_update_stamp_tracks_applied_updates_not_wall_time() {
  // Semantics of the "upd HH:MM" stamp: it must show the time of the last
  // ACTUALLY applied panel update — never advance just because minutes pass.
  WarmFixture fx(/*wifi_ok=*/true, /*http_ok=*/true, /*synced=*/true);
  fx.renderer.freeze = true; // identical board content on every render
  CycleDeps deps = fx.deps();

  // Cycle 1: no previous framebuffer → a refresh is applied → stamp = now.
  runWarmCycle(deps, fx.meta);
  const time_t t1 = fx.meta.last_display_update;
  TEST_ASSERT_EQUAL_INT64(kSyncedNow, t1);
  const int draws_after_c1 = fx.display.draw_partial_calls +
                             fx.display.light_full_calls +
                             fx.display.deep_clean_calls;
  TEST_ASSERT_GREATER_THAN(0, draws_after_c1);

  // Cycle 2, five minutes later, identical content: nothing may reach the
  // panel and the stamp must NOT tick to the new time.
  fx.clock.advance(300);
  runWarmCycle(deps, fx.meta);
  TEST_ASSERT_EQUAL_INT64(t1, fx.meta.last_display_update);
  const int draws_after_c2 = fx.display.draw_partial_calls +
                             fx.display.light_full_calls +
                             fx.display.deep_clean_calls;
  TEST_ASSERT_EQUAL(draws_after_c1, draws_after_c2);

  // Cycle 3, content changes: a refresh is applied and the stamp follows.
  fx.renderer.freeze = false;
  fx.clock.advance(300);
  runWarmCycle(deps, fx.meta);
  TEST_ASSERT_EQUAL_INT64(kSyncedNow + 600, fx.meta.last_display_update);
  const int draws_after_c3 = fx.display.draw_partial_calls +
                             fx.display.light_full_calls +
                             fx.display.deep_clean_calls;
  TEST_ASSERT_GREATER_THAN(draws_after_c2, draws_after_c3);
}

void test_force_stamp_advances_stamp_on_unchanged_data() {
  // Button press (force_stamp=true): even with byte-identical board content,
  // the "upd HH:MM" stamp must advance and a refresh must reach the panel, so
  // the press gives visible feedback. This is the boot-button "nothing happens
  // on unchanged data" fix.
  WarmFixture fx(/*wifi_ok=*/true, /*http_ok=*/true, /*synced=*/true);
  fx.renderer.freeze = true; // identical board content on every render
  CycleDeps deps = fx.deps();

  // Cycle 1: no previous framebuffer → a refresh is applied → stamp = now.
  runWarmCycle(deps, fx.meta);
  const time_t t1 = fx.meta.last_display_update;
  TEST_ASSERT_EQUAL_INT64(kSyncedNow, t1);
  const int draws_after_c1 = fx.display.draw_partial_calls +
                             fx.display.light_full_calls +
                             fx.display.deep_clean_calls;

  // Cycle 2, five minutes later, identical content, forced (button press):
  // the stamp MUST tick to the new time and a draw MUST reach the panel.
  fx.clock.advance(300);
  runWarmCycle(deps, fx.meta, CycleTrigger::Button);
  TEST_ASSERT_EQUAL_INT64(kSyncedNow + 300, fx.meta.last_display_update);
  const int draws_after_c2 = fx.display.draw_partial_calls +
                             fx.display.light_full_calls +
                             fx.display.deep_clean_calls;
  TEST_ASSERT_GREATER_THAN(draws_after_c1, draws_after_c2);
}

// --- Deep-wake panel-RAM guard -------------------------------------------
//
// Field bug: after a timer (deep-sleep) wake, a small content change produced a
// PARTIAL refresh. On the UC8176 fast-partial panel the on-glass differential
// RAM was gone, so everything outside the freshly-written bbox came back as
// stale garbage — "thick white borders top/left, garbled middle, one clean
// block lower-right". The fix promotes the first post-deep-wake refresh to a
// full. These two tests pin the end-to-end wiring (deps.deep_wake →
// renderAndPush → planRefresh), not just the planner unit.

// Runs two cycles: the first seeds a valid persisted framebuffer and a recent
// last_light_full; the second changes one pixel. Returns the fixture so the
// caller can inspect which refresh op the second cycle issued.
static void seedValidPrevFrame(WarmFixture &fx) {
  // Cycle 1 (not the one under test): renders, saves a framebuffer, and — via
  // the light-full it performs on the first-ever (invalid-prev) refresh — sets
  // last_light_full to now, so the time-trigger won't fire next cycle.
  CycleDeps warmup = fx.deps(/*deep_wake=*/false);
  runWarmCycle(warmup, fx.meta);
}

void test_deep_wake_promotes_small_change_to_light_full() {
  WarmFixture fx(/*wifi_ok=*/true, /*http_ok=*/true, /*synced=*/true);
  seedValidPrevFrame(fx);
  const int lf_before = fx.display.light_full_calls;
  const int pt_before = fx.display.draw_partial_calls;

  // Second cycle: renderer advances one pixel (freeze stays false) → the diff
  // is a tiny partial. deep_wake=true must promote it to a light-full.
  CycleDeps deps = fx.deps(/*deep_wake=*/true);
  runWarmCycle(deps, fx.meta);

  TEST_ASSERT_EQUAL(pt_before, fx.display.draw_partial_calls); // no partial
  TEST_ASSERT_EQUAL(lf_before + 1, fx.display.light_full_calls);
}

void test_active_phase_small_change_stays_partial() {
  // Control: same small change, but deep_wake=false (active-phase light-sleep
  // poll, panel still powered) must stay a cheap partial.
  WarmFixture fx(/*wifi_ok=*/true, /*http_ok=*/true, /*synced=*/true);
  seedValidPrevFrame(fx);
  const int lf_before = fx.display.light_full_calls;
  const int pt_before = fx.display.draw_partial_calls;

  CycleDeps deps = fx.deps(/*deep_wake=*/false);
  runWarmCycle(deps, fx.meta);

  TEST_ASSERT_EQUAL(pt_before + 1, fx.display.draw_partial_calls);
  TEST_ASSERT_EQUAL(lf_before, fx.display.light_full_calls); // no extra full
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_warm_happy_renders_and_saves_before_sleep);
  RUN_TEST(test_warm_happy_stamps_cycle_trace);
  RUN_TEST(test_warm_button_trigger_recorded);
  RUN_TEST(test_warm_wifi_down_records_anomaly);
  RUN_TEST(test_warm_wifi_down_skips_render_and_deepSleeps_poll_interval);
  RUN_TEST(test_warm_wifi_down_old_data_renders_offline);
  RUN_TEST(test_warm_unsynced_clock_forces_ntp_then_continues);
  RUN_TEST(test_warm_fetch_fail_prestale_keeps_last_frame);
  RUN_TEST(test_warm_fetch_fail_old_data_keeps_last_frame);
  RUN_TEST(test_warm_clock_jumped_hours_ahead_forces_resync);
  RUN_TEST(test_warm_clock_lands_on_wake_target_does_not_resync);
  RUN_TEST(test_warm_first_boot_no_wake_reference_does_not_resync);
  RUN_TEST(test_update_stamp_tracks_applied_updates_not_wall_time);
  RUN_TEST(test_force_stamp_advances_stamp_on_unchanged_data);
  RUN_TEST(test_deep_wake_promotes_small_change_to_light_full);
  RUN_TEST(test_active_phase_small_change_stays_partial);
  return UNITY_END();
}
