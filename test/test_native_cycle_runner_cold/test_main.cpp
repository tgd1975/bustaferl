// Tier 2 — call-sequence recording for runColdCycle. Cold path is rarer in
// practice but the most dangerous failure mode: a bad cold cycle leaves the
// device in a guru-meditation loop without a panel update. Three variants:
// happy (Ok) vs the no-wifi loop (first paint, refresh, counter cap).

#include "../test_native_cycle_runner_warm/recording_fakes.h"
#include "logic/cycle_runner.h"

#include <cstdio>
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

  ColdFixture(bool wifi_ok, bool http_ok, uint8_t retries_so_far,
              uint8_t no_wifi_cycles = 0)
      : clock(trace, kSyncedNow, /*synced=*/wifi_ok),
        net(trace, wifi_ok, http_ok, "{}"), sleep(trace, WakeCause::ColdBoot),
        store(trace), display(trace), renderer(trace) {
    cfg.api_base = "http://api/";
    cfg.efa_base = "http://efa/";
    meta.cold_boot_retries = retries_so_far;
    meta.no_wifi_cycles = no_wifi_cycles;
  }

  CycleDeps deps() {
    return CycleDeps{clock,    net,  sleep, store, display,
                     renderer, curr, prev,  cfg};
  }
};

} // namespace

void setUp() {}
void tearDown() {}

void test_cold_happy_deep_cleans_and_renders() {
  ColdFixture fx(/*wifi_ok=*/true, /*http_ok=*/true, /*retries=*/0);
  CycleDeps deps = fx.deps();
  runColdCycle(deps, fx.meta);

  // With no trusted frame on glass, the cold path opens with the Boot screen
  // (render + lightFull) so the user sees "loading…" before WiFi/NTP run, then
  // renders the real board. Two renderer.render calls (boot + board). The
  // boot-check dashboard (default boot_info_show_s=15) deep-cleans once, and
  // both the boot screen and finishColdCycle lightFull → two lightFulls.
  TEST_ASSERT_EQUAL(2, fx.renderer.calls);
  TEST_ASSERT_EQUAL(2, fx.display.light_full_calls);
  TEST_ASSERT_EQUAL(1, fx.display.deep_clean_calls);
  TEST_ASSERT_EQUAL(0, fx.display.draw_partial_calls);
  TEST_ASSERT_EQUAL(1, fx.sleep.deep_sleep_calls);
  TEST_ASSERT_TRUE(fx.meta.framebuffer_valid);
  // last_deep_clean stamped.
  TEST_ASSERT_EQUAL_INT64(kSyncedNow, fx.meta.last_deep_clean);
  // cold_boot_retries reset after success.
  TEST_ASSERT_EQUAL(0, fx.meta.cold_boot_retries);
}

// First cold attempt with WiFi down (no_wifi_cycles == 0): boot screen paints
// first, then the no-network screen deep-cleans over it (first appearance), the
// no-wifi counter advances, and the device sleeps the 60 s retry interval — NOT
// a longer give-up backoff.
void test_cold_no_wifi_first_paints_screen_and_retries_in_60s() {
  ColdFixture fx(/*wifi_ok=*/false, /*http_ok=*/true, /*retries=*/0);
  CycleDeps deps = fx.deps();
  runColdCycle(deps, fx.meta);

  // Boot screen (lightFull) + KEIN EMPFANG (deepClean) = 2 renders.
  TEST_ASSERT_EQUAL(2, fx.renderer.calls);
  TEST_ASSERT_EQUAL(DisplayState::Offline, fx.renderer.last_state);
  TEST_ASSERT_EQUAL(1, fx.display.light_full_calls); // boot screen
  TEST_ASSERT_EQUAL(1, fx.display.deep_clean_calls); // first KEIN EMPFANG
  TEST_ASSERT_EQUAL(1, fx.meta.no_wifi_cycles);      // 0 -> 1
  TEST_ASSERT_EQUAL(1, fx.sleep.deep_sleep_calls);
  TEST_ASSERT_EQUAL_UINT(fx.cfg.cold_boot_retry_s,
                         fx.sleep.last_deep_sleep_seconds);
}

// A no-wifi cycle that is NOT on a repaint boundary (no_wifi_cycles % 5 != 0):
// retry WiFi + sleep 60 s, but leave the panel untouched. The screen from the
// last repaint stays up.
void test_cold_no_wifi_between_repaints_does_not_touch_panel() {
  ColdFixture fx(/*wifi_ok=*/false, /*http_ok=*/true, /*retries=*/2,
                 /*no_wifi_cycles=*/2);
  CycleDeps deps = fx.deps();
  runColdCycle(deps, fx.meta);

  // Boot screen suppressed (retries > 0) and no repaint → nothing drawn.
  TEST_ASSERT_EQUAL(0, fx.renderer.calls);
  TEST_ASSERT_EQUAL(0, fx.display.light_full_calls);
  TEST_ASSERT_EQUAL(0, fx.display.deep_clean_calls);
  TEST_ASSERT_EQUAL(3, fx.meta.no_wifi_cycles); // still advances 2 -> 3
  TEST_ASSERT_EQUAL_UINT(fx.cfg.cold_boot_retry_s,
                         fx.sleep.last_deep_sleep_seconds); // still retries
}

// A no-wifi cycle ON a repaint boundary (no_wifi_cycles == 5): repaint with a
// light single-flash (not a deep clean — that is reserved for the first paint).
void test_cold_no_wifi_repaint_boundary_is_light_full() {
  ColdFixture fx(/*wifi_ok=*/false, /*http_ok=*/true, /*retries=*/5,
                 /*no_wifi_cycles=*/5);
  CycleDeps deps = fx.deps();
  runColdCycle(deps, fx.meta);

  TEST_ASSERT_EQUAL(1, fx.renderer.calls); // only KEIN EMPFANG
  TEST_ASSERT_EQUAL(DisplayState::Offline, fx.renderer.last_state);
  TEST_ASSERT_EQUAL(1, fx.display.light_full_calls); // refresh, not deep clean
  TEST_ASSERT_EQUAL(0, fx.display.deep_clean_calls);
  TEST_ASSERT_EQUAL(6, fx.meta.no_wifi_cycles); // 5 -> 6
  TEST_ASSERT_EQUAL_UINT(fx.cfg.cold_boot_retry_s,
                         fx.sleep.last_deep_sleep_seconds);
}

// A successful boot resets the no-wifi counter so the next outage starts its
// repaint cadence fresh.
void test_cold_success_resets_no_wifi_counter() {
  ColdFixture fx(/*wifi_ok=*/true, /*http_ok=*/true, /*retries=*/0,
                 /*no_wifi_cycles=*/3);
  CycleDeps deps = fx.deps();
  runColdCycle(deps, fx.meta);

  TEST_ASSERT_EQUAL(0, fx.meta.no_wifi_cycles);
}

// The no-network screen carries both the SSIDs the failed scan saw and the
// SSIDs the device was looking for, so a field diagnosis can read the mismatch
// off the panel.
void test_cold_no_wifi_screen_carries_visible_ssids() {
  ColdFixture fx(/*wifi_ok=*/false, /*http_ok=*/true,
                 /*retries=*/DEFAULT_COLD_BOOT_MAX_RETRIES);
  ScanResult scan;
  scan.count = 2;
  std::snprintf(scan.aps[0].ssid, sizeof(scan.aps[0].ssid), "%s", "A-NET2");
  scan.aps[0].rssi_dbm = -67;
  std::snprintf(scan.aps[1].ssid, sizeof(scan.aps[1].ssid), "%s", "Nachbar");
  scan.aps[1].rssi_dbm = -80;
  fx.net.seedScan(scan);

  ConfiguredSsids wanted;
  wanted.count = 1;
  std::snprintf(wanted.ssid[0], sizeof(wanted.ssid[0]), "%s", "Zuhause-WLAN");
  fx.net.seedConfigured(wanted);

  CycleDeps deps = fx.deps();
  runColdCycle(deps, fx.meta);

  TEST_ASSERT_EQUAL(DisplayState::Offline, fx.renderer.last_state);
  TEST_ASSERT_EQUAL(2, fx.renderer.last_visible_aps.count);
  TEST_ASSERT_EQUAL_STRING("A-NET2", fx.renderer.last_visible_aps.aps[0].ssid);
  TEST_ASSERT_EQUAL_STRING("Nachbar", fx.renderer.last_visible_aps.aps[1].ssid);
  TEST_ASSERT_EQUAL(1, fx.renderer.last_wanted_ssids.count);
  TEST_ASSERT_EQUAL_STRING("Zuhause-WLAN",
                           fx.renderer.last_wanted_ssids.ssid[0]);
  // Configured name shares nothing with the visible ones → no case-mismatch.
  TEST_ASSERT_FALSE(fx.renderer.last_case_mismatch.found);
}

// The exact field case: configured "A-NET2" while the AP broadcasts "a-net2".
// The no-network render must carry the case-mismatch hint to the panel.
void test_cold_no_wifi_screen_flags_case_mismatch() {
  ColdFixture fx(/*wifi_ok=*/false, /*http_ok=*/true,
                 /*retries=*/DEFAULT_COLD_BOOT_MAX_RETRIES);
  ScanResult scan;
  scan.count = 1;
  std::snprintf(scan.aps[0].ssid, sizeof(scan.aps[0].ssid), "%s", "a-net2");
  scan.aps[0].rssi_dbm = -63;
  fx.net.seedScan(scan);

  ConfiguredSsids wanted;
  wanted.count = 1;
  std::snprintf(wanted.ssid[0], sizeof(wanted.ssid[0]), "%s", "A-NET2");
  fx.net.seedConfigured(wanted);

  CycleDeps deps = fx.deps();
  runColdCycle(deps, fx.meta);

  TEST_ASSERT_EQUAL(DisplayState::Offline, fx.renderer.last_state);
  TEST_ASSERT_TRUE(fx.renderer.last_case_mismatch.found);
  TEST_ASSERT_EQUAL_STRING("A-NET2", fx.renderer.last_case_mismatch.configured);
  TEST_ASSERT_EQUAL_STRING("a-net2", fx.renderer.last_case_mismatch.visible);
}

// Wrong WiFi password (WPA handshake failed): terminal. The cold cycle renders
// the dedicated WifiAuth screen naming the SSID, deep-cleans, resets the retry
// counter, and sleeps the LONG auth interval — not the 60 s no-network loop.
void test_cold_wrong_password_is_terminal() {
  ColdFixture fx(/*wifi_ok=*/false, /*http_ok=*/true, /*retries=*/0);
  fx.net.seedFailure(WifiFailure::AuthFailed);
  ConfiguredSsids wanted;
  wanted.count = 1;
  std::snprintf(wanted.ssid[0], sizeof(wanted.ssid[0]), "%s", "a-net2");
  fx.net.seedConfigured(wanted);

  CycleDeps deps = fx.deps();
  runColdCycle(deps, fx.meta);

  TEST_ASSERT_EQUAL(DisplayState::WifiAuth, fx.renderer.last_state);
  TEST_ASSERT_EQUAL_STRING("a-net2", fx.renderer.last_wanted_ssids.ssid[0]);
  TEST_ASSERT_EQUAL(1, fx.display.deep_clean_calls);
  TEST_ASSERT_EQUAL(0, fx.meta.cold_boot_retries); // not a retry-loop advance
  TEST_ASSERT_EQUAL_UINT(fx.cfg.wifi_auth_sleep_s,
                         fx.sleep.last_deep_sleep_seconds);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_cold_happy_deep_cleans_and_renders);
  RUN_TEST(test_cold_no_wifi_first_paints_screen_and_retries_in_60s);
  RUN_TEST(test_cold_no_wifi_between_repaints_does_not_touch_panel);
  RUN_TEST(test_cold_no_wifi_repaint_boundary_is_light_full);
  RUN_TEST(test_cold_success_resets_no_wifi_counter);
  RUN_TEST(test_cold_no_wifi_screen_carries_visible_ssids);
  RUN_TEST(test_cold_no_wifi_screen_flags_case_mismatch);
  RUN_TEST(test_cold_wrong_password_is_terminal);
  return UNITY_END();
}
