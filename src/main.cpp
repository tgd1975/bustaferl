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
  // A cold-boot retry exits via deepSleep(), so the wake comes back as a Timer,
  // not ColdBoot. Two cases keep us on the cold path on a Timer wake:
  //   1. cold_boot_retries > 0 — still inside the cold retry loop; the counter
  //      must keep climbing rather than ping-ponging through warm cycles.
  //   2. !has_any_data — the device has never completed a single fetch, so it
  //      has no board to show. It stays on the cold path until the first
  //      success flips has_any_data: boot screen → (WiFi down) KEIN EMPFANG,
  //      re-scanned + repainted every COLD_BOOT_RETRY_S (60 s), until WiFi
  //      appears and the next cold cycle connects and runs the full sequence.
  //      Without this the next Timer wake would fall through to a warm cycle
  //      that renders nothing and polls every 30 s.
  const bool cold_boot_pending =
      cause == WakeCause::Timer &&
      (meta.cold_boot_retries > 0 || !meta.has_any_data);
  if (cause == WakeCause::ColdBoot || cold_boot_pending) {
    Serial.println("[boot] cold");
    runColdCycle(deps, meta);
  } else if (cause == WakeCause::Button) {
    runButtonWake(deps, g_button, meta);
  } else {
    Serial.println("[boot] warm");
    runWarmCycle(deps, meta);
  }
}

void loop() {
  // Active-phase polling: runWarmCycle set us up for
  // lightSleep(POLL_INTERVAL_S) and we get here when it returns — either
  // on timer, or because the boot button was pressed (configured as a GPIO
  // wake source). Check the button first so a long press during active
  // mode still resets the panel.
  PersistedMeta meta = g_store.loadMeta();
  CycleDeps deps = makeDeps();
  pollButtonAndRunWarm(deps, g_button, meta);
}

#endif // NATIVE_BUILD
