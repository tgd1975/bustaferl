// Empty in env:native (NATIVE_BUILD set by host build_flags). The native env's
// build_src_filter already drops production .cpp's, but this gate is a second
// line of defence: any future env that picks up src/ broadly won't drag the
// Arduino-only main into a host link.
#ifndef NATIVE_BUILD

#include "config.h"
#include "hal/Esp32Button.h"
#include "hal/Esp32Clock.h"
#include "hal/Esp32Display.h"
#include "hal/Esp32Network.h"
#include "hal/Esp32PersistentStore.h"
#include "hal/Esp32Renderer.h"
#include "hal/Esp32Sleep.h"
#include "logic/cycle_runner.h"
#include "secrets.h"

#include <Arduino.h>

using namespace bustaferl;

namespace {

Esp32Clock g_clock{NTP_SERVER, TZ_INFO};
Esp32Network g_net;
Esp32Sleep g_sleep;
Esp32PersistentStore g_store;
Esp32Display g_display;
Esp32Button g_button{BTN_BOOT_PIN};
Esp32Renderer g_renderer;

Frame g_frame_new;
Frame g_frame_prev;

CycleConfig makeCycleConfig() {
  CycleConfig c;
  c.api_base = WL_API_BASE;
  c.efa_base = WL_EFA_DM_BASE;
  c.mgate_url = OEBB_MGATE_URL;
  c.wifi_connect_ms = 10000;
  c.cold_boot_retry_s = COLD_BOOT_RETRY_S;
  c.no_wifi_repaint_every = NO_WIFI_REPAINT_EVERY;
  c.cold_boot_max_retries = COLD_BOOT_MAX_RETRIES;
  c.poll_interval_s = POLL_INTERVAL_S;
  c.stale_threshold_s = STALE_THRESHOLD_S;
  c.ntp_interval_s = NTP_INTERVAL_S;
  c.filter_health_dead_after = FILTER_HEALTH_DEAD_AFTER;
  c.wake_before_bus_s = WAKE_BEFORE_BUS_S;
  c.boot_margin_s = BOOT_MARGIN_S;
  c.active_threshold_s = ACTIVE_THRESHOLD_S;
  c.no_data_sleep_s = NO_DATA_SLEEP_S;
  c.api_failure_retry_s = API_FAILURE_RETRY_S;
  c.long_sleep_for_nightly_clean_s = LONG_SLEEP_FOR_NIGHTLY_CLEAN_S;
  c.nightly_deep_clean_interval_s = NIGHTLY_DEEP_CLEAN_INTERVAL_S;
  c.btn_long_press_ms = BTN_LONG_PRESS_MS;
  c.btn_double_click_ms = BTN_DOUBLE_CLICK_MS;
  c.diag_max_s = DIAG_MAX_S;
  c.boot_info_show_s = BOOT_INFO_SHOW_S;
  c.brownout_show_s = BROWNOUT_SHOW_S;
  c.rescue_window_start_s = RESCUE_WINDOW_START_S;
  c.rescue_window_end_s = RESCUE_WINDOW_END_S;
  c.rescue_retry_pause_s = RESCUE_RETRY_PAUSE_S;
  c.rescue_max_attempts = RESCUE_MAX_ATTEMPTS;
  return c;
}

const CycleConfig g_cycle_cfg = makeCycleConfig();

// `deep_wake` marks a setup()-entry cycle (fresh deep-sleep wake): the panel's
// on-glass RAM is untrusted so the first refresh is forced full. The loop()
// active phase reuses the powered panel across light sleep, so it leaves the
// default false and keeps partials.
CycleDeps makeDeps(bool deep_wake = false) {
  return CycleDeps{g_clock,     g_net,      g_sleep,     g_store,
                   g_display,   g_renderer, g_frame_new, g_frame_prev,
                   g_cycle_cfg, deep_wake};
}

void registerWifiCredentials() {
  g_net.addAp(WIFI_SSID_PRIMARY, WIFI_PASSWORD_PRIMARY);
#ifdef WIFI_SSID_SECONDARY
  g_net.addAp(WIFI_SSID_SECONDARY, WIFI_PASSWORD_SECONDARY);
#endif
}

} // namespace

void setup() {
  Serial.begin(115200);
  delay(100);

  g_button.init();

  registerWifiCredentials();
  g_display.init();

  PersistedMeta meta = g_store.loadMeta();
  // setup() always runs after a deep-sleep wake (ESP32 reboots from cold) —
  // the panel's differential RAM is untrusted, so force the first refresh full.
  CycleDeps deps = makeDeps(/*deep_wake=*/true);

  WakeCause cause = g_sleep.wakeupCause();
  // Real reset cause (brownout vs watchdog/panic vs a genuine power-on or
  // deep-sleep wake) — distinct from WakeCause, which cannot tell those
  // apart (see selectCycle()'s routing comment below). Persisted so the
  // STATUS diagnostic page can show it after this one-shot overlay is gone.
  ResetReason reset_reason = g_sleep.lastResetReason();
  meta.last_reset_reason = reset_reason;
  showBrownoutScreen(deps, reset_reason);

  // Routing lives in selectCycle() (logic/cycle_runner) so it is host-testable
  // — main.cpp is excluded from the native build. The cold (boot-screen) path
  // runs only while the device has no board to show yet (never fetched, or
  // still inside the cold retry loop). Crucially, a non-deep-sleep reset during
  // warm operation (brownout on a WiFi-current spike, watchdog, panic) reports
  // as ColdBoot but leaves RTC memory intact, so has_any_data is still true and
  // selectCycle sends it to a warm cycle — no boot screen flashing mid-run.
  switch (selectCycle(cause, meta.cold_boot_retries, meta.has_any_data)) {
  case CycleKind::Cold:
    Serial.println("[boot] cold");
    runColdCycle(deps, meta);
    break;
  case CycleKind::Button:
    runButtonWake(deps, g_button, meta);
    break;
  case CycleKind::Warm:
    Serial.println("[boot] warm");
    runWarmCycle(deps, meta);
    break;
  }
}

void loop() {
  // Active-phase polling: runWarmCycle set us up for
  // pause(POLL_INTERVAL_S) and we get here when it returns — either because
  // the interval elapsed, or because the boot button was pressed (pause()
  // polls the pin and returns early). Check the button first so a long press
  // during active mode still resets the panel.
  PersistedMeta meta = g_store.loadMeta();
  CycleDeps deps = makeDeps();
  pollButtonAndRunWarm(deps, g_button, meta);
}

#endif // NATIVE_BUILD
