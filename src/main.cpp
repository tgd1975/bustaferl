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
  c.cold_boot_giveup_sleep_s = 300;
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
  c.rescue_window_start_s = RESCUE_WINDOW_START_S;
  c.rescue_window_end_s = RESCUE_WINDOW_END_S;
  c.rescue_retry_pause_s = RESCUE_RETRY_PAUSE_S;
  c.rescue_max_attempts = RESCUE_MAX_ATTEMPTS;
  return c;
}

const CycleConfig g_cycle_cfg = makeCycleConfig();

CycleDeps makeDeps() {
  return CycleDeps{g_clock,    g_net,       g_sleep,      g_store,    g_display,
                   g_renderer, g_frame_new, g_frame_prev, g_cycle_cfg};
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
  CycleDeps deps = makeDeps();

  WakeCause cause = g_sleep.wakeupCause();
  if (cause == WakeCause::ColdBoot) {
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
