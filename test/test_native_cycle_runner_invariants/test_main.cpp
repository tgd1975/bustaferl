// Tier 3 — property tests for cycle_runner. The bug class these catch is
// the one that bit us before the refactor: "warmCyclePath computes a 50-
// year deepSleep when the clock is unsynced" / "saveMeta gets called twice
// per cycle through copy/paste drift". Instead of pinning specific traces
// (Tier 2), assert structural invariants across a small matrix of states.

#include "../test_native_cycle_runner_warm/recording_fakes.h"
#include "logic/cycle_runner.h"

#include <unity.h>

using namespace bustaferl;
using namespace bustaferl::test;

namespace {

constexpr time_t kSyncedNow = 1736000000;
// Practical upper bound on a single sleep — see CONCEPT.md §6.
constexpr unsigned MAX_REASONABLE_DEEP_SLEEP_S = 86400;

struct Probe {
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

  Probe(bool wifi_ok, bool http_ok, bool synced, WakeCause cause)
      : clock(trace, kSyncedNow, synced), net(trace, wifi_ok, http_ok, "{}"),
        sleep(trace, cause), store(trace), display(trace), renderer(trace) {
    cfg.api_base = "http://api/";
    cfg.efa_base = "http://efa/";
    meta.last_ntp_sync = kSyncedNow - 60;
    meta.last_api_success = kSyncedNow - 30;
  }

  CycleDeps deps() {
    return CycleDeps{clock,    net,  sleep, store, display,
                     renderer, curr, prev,  cfg};
  }
};

} // namespace

void setUp() {}
void tearDown() {}

// Property: warm cycle never deep-sleeps for more than 24 h across every
// permutation of (wifi_ok × http_ok × synced). The historic regression was
// a 50-year value coming out of planSleep when now()=0.
void test_warm_never_deep_sleeps_beyond_24h() {
  for (bool wifi_ok : {false, true}) {
    for (bool http_ok : {false, true}) {
      for (bool synced : {false, true}) {
        Probe p(wifi_ok, http_ok, synced, WakeCause::Timer);
        CycleDeps deps = p.deps();
        runWarmCycle(deps, p.meta);
        if (p.sleep.deep_sleep_calls == 0)
          continue;
        TEST_ASSERT_LESS_OR_EQUAL_UINT(MAX_REASONABLE_DEEP_SLEEP_S,
                                       p.sleep.last_deep_sleep_seconds);
      }
    }
  }
}

// Property: saveMeta is called at most once per warm cycle. Two calls means
// we forgot to remove a copy-paste from the old code, three means the
// nightly + normal paths are double-firing.
void test_warm_saves_meta_at_most_once() {
  for (bool wifi_ok : {false, true}) {
    for (bool http_ok : {false, true}) {
      Probe p(wifi_ok, http_ok, /*synced=*/true, WakeCause::Timer);
      CycleDeps deps = p.deps();
      runWarmCycle(deps, p.meta);
      // 0 is acceptable on certain abort paths (wifi-down non-stale path
      // *does* save meta via doSleepOrLoop; wifi-down stale also via
      // doSleepOrLoop). All non-abort branches save exactly once.
      TEST_ASSERT_LESS_OR_EQUAL_INT(1, p.store.save_meta_calls);
    }
  }
}

// Property: warm cycle calls renderer.render at most once per cycle. Double-
// rendering would burn the panel (each render is followed by a display
// update) and used to happen in the old nightly path (renderFrame then
// renderAndPush which re-rendered).
void test_warm_renders_at_most_once() {
  for (bool wifi_ok : {false, true}) {
    for (bool http_ok : {false, true}) {
      Probe p(wifi_ok, http_ok, /*synced=*/true, WakeCause::Timer);
      CycleDeps deps = p.deps();
      runWarmCycle(deps, p.meta);
      TEST_ASSERT_LESS_OR_EQUAL_INT(1, p.renderer.calls);
    }
  }
}

// Property: cold cycle always deep-cleans the panel exactly once on the
// happy path. Deep-clean is what guarantees a fresh-from-factory panel
// after a power loss — skipping it leaves ghosting from a previous owner.
void test_cold_happy_always_deep_cleans_exactly_once() {
  Probe p(/*wifi_ok=*/true, /*http_ok=*/true, /*synced=*/true,
          WakeCause::ColdBoot);
  CycleDeps deps = p.deps();
  runColdCycle(deps, p.meta);
  TEST_ASSERT_EQUAL(1, p.display.deep_clean_calls);
}

// Property: light-sleep is the active-phase tool only. The warm cycle in
// the steady-state (next-bus-far-away) path must deepSleep, not lightSleep.
// A lightSleep here would burn battery during the overnight pause.
void test_warm_in_quiet_period_uses_deep_sleep() {
  Probe p(/*wifi_ok=*/true, /*http_ok=*/true, /*synced=*/true,
          WakeCause::Timer);
  CycleDeps deps = p.deps();
  runWarmCycle(deps, p.meta);
  // With an empty StreamSnapshot (the parse of "{}" produces nothing
  // departable), planSleep falls back to NO_DATA_SLEEP_S — a deepSleep.
  TEST_ASSERT_EQUAL(0, p.sleep.light_sleep_calls);
  TEST_ASSERT_EQUAL(1, p.sleep.deep_sleep_calls);
}

// --- selectCycle: wake-cause → cycle routing (Bug: boot screen mid-op) ------

// The regression: a brownout/watchdog/panic reset during warm operation is
// reported by the SDK as ColdBoot (ESP_SLEEP_WAKEUP_UNDEFINED) but leaves RTC
// memory — and thus has_any_data — intact. Routing that to the cold path
// re-flashed the boot screen mid-operation. With data present it must go warm.
void test_reset_with_data_routes_warm_no_bootscreen() {
  TEST_ASSERT_TRUE(CycleKind::Warm == selectCycle(WakeCause::ColdBoot,
                                                  /*retries=*/0,
                                                  /*has_any_data=*/true));
}

// A genuine power-on wipes RTC: has_any_data is false → real cold boot.
void test_first_power_on_routes_cold() {
  TEST_ASSERT_TRUE(CycleKind::Cold == selectCycle(WakeCause::ColdBoot,
                                                  /*retries=*/0,
                                                  /*has_any_data=*/false));
}

// Inside the cold retry loop (retries>0) the wake comes back as a Timer; it
// must stay cold so the boot sequence / counter keep running, even though no
// board exists yet.
void test_cold_retry_loop_stays_cold_on_timer() {
  TEST_ASSERT_TRUE(CycleKind::Cold == selectCycle(WakeCause::Timer,
                                                  /*retries=*/2,
                                                  /*has_any_data=*/false));
}

// Steady state: routine timer wake with data present → warm.
void test_timer_with_data_routes_warm() {
  TEST_ASSERT_TRUE(CycleKind::Warm == selectCycle(WakeCause::Timer,
                                                  /*retries=*/0,
                                                  /*has_any_data=*/true));
}

// A button press always classifies as Button regardless of data state, so a
// press on a never-fetched device still enters the button handler.
void test_button_always_routes_button() {
  for (bool has_data : {false, true}) {
    for (uint8_t retries : {uint8_t{0}, uint8_t{3}}) {
      TEST_ASSERT_TRUE(CycleKind::Button ==
                       selectCycle(WakeCause::Button, retries, has_data));
    }
  }
}

// Property: the boot-screen (Cold) path is chosen ONLY when the device has no
// board to show yet — never once has_any_data is true and no retry is pending.
// This is the core invariant that keeps the boot screen out of warm operation.
void test_cold_only_when_no_board() {
  for (WakeCause cause :
       {WakeCause::ColdBoot, WakeCause::Timer, WakeCause::Other}) {
    for (uint8_t retries : {uint8_t{0}, uint8_t{1}, uint8_t{5}}) {
      for (bool has_data : {false, true}) {
        const bool cold =
            selectCycle(cause, retries, has_data) == CycleKind::Cold;
        const bool needs_cold = retries > 0 || !has_data;
        TEST_ASSERT_EQUAL(needs_cold, cold);
      }
    }
  }
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_warm_never_deep_sleeps_beyond_24h);
  RUN_TEST(test_warm_saves_meta_at_most_once);
  RUN_TEST(test_warm_renders_at_most_once);
  RUN_TEST(test_cold_happy_always_deep_cleans_exactly_once);
  RUN_TEST(test_warm_in_quiet_period_uses_deep_sleep);
  RUN_TEST(test_reset_with_data_routes_warm_no_bootscreen);
  RUN_TEST(test_first_power_on_routes_cold);
  RUN_TEST(test_cold_retry_loop_stays_cold_on_timer);
  RUN_TEST(test_timer_with_data_routes_warm);
  RUN_TEST(test_button_always_routes_button);
  RUN_TEST(test_cold_only_when_no_board);
  return UNITY_END();
}
