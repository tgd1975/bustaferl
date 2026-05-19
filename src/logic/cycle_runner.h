#ifndef BUSTAFERL_CYCLE_RUNNER_H
#define BUSTAFERL_CYCLE_RUNNER_H

#include "../hal/IButton.h"
#include "../hal/IClock.h"
#include "../hal/IDisplay.h"
#include "../hal/INetwork.h"
#include "../hal/IPersistentStore.h"
#include "../hal/IRenderer.h"
#include "../hal/ISleep.h"
#include "../render/layout.h"
#include "boot_sequencer.h" // DEFAULT_WIFI_TIMEOUT_MS, DEFAULT_COLD_BOOT_MAX_RETRIES
#include "sleep_planner.h" // DEFAULT_WAKE_BEFORE_BUS_S, …

#include <string>

namespace bustaferl {

// Production defaults — mirror the macros in config.h. main.cpp can override
// from those macros at startup; host tests use the defaults so a freshly-
// constructed CycleConfig is sufficient. Where an existing module already
// defines the constant (boot_sequencer, sleep_planner) we reuse that as the
// single source of truth.
constexpr unsigned DEFAULT_COLD_BOOT_RETRY_S = 60;
constexpr unsigned DEFAULT_COLD_BOOT_GIVEUP_SLEEP_S = 300;
constexpr unsigned DEFAULT_POLL_INTERVAL_S = 30;
constexpr int DEFAULT_STALE_THRESHOLD_S = 180;
constexpr int DEFAULT_NTP_INTERVAL_S = 86400;
constexpr uint8_t DEFAULT_FILTER_HEALTH_DEAD_AFTER = 3;
constexpr unsigned DEFAULT_LONG_SLEEP_FOR_NIGHTLY_CLEAN_S = 4U * 3600U;
constexpr int DEFAULT_NIGHTLY_DEEP_CLEAN_INTERVAL_S = 20 * 3600;
constexpr unsigned DEFAULT_BTN_LONG_PRESS_MS = 2000;

// Endpoint URLs + tunables the cycle reads. Defaults reflect the production
// values in config.h; host tests instantiate with empty strings and the
// recording-fake INetwork ignores the URL contents.
struct CycleConfig {
  std::string api_base;  // Wiener Linien OGD realtime endpoint
  std::string efa_base;  // Wiener Linien EFA schedule endpoint
  std::string mgate_url; // ÖBB HAFAS mgate.exe (v2 S-Bahn stream)
  unsigned wifi_connect_ms = DEFAULT_WIFI_TIMEOUT_MS;
  unsigned cold_boot_retry_s = DEFAULT_COLD_BOOT_RETRY_S;
  unsigned cold_boot_giveup_sleep_s = DEFAULT_COLD_BOOT_GIVEUP_SLEEP_S;
  uint8_t cold_boot_max_retries = DEFAULT_COLD_BOOT_MAX_RETRIES;
  unsigned poll_interval_s = DEFAULT_POLL_INTERVAL_S;
  int stale_threshold_s = DEFAULT_STALE_THRESHOLD_S;
  int ntp_interval_s = DEFAULT_NTP_INTERVAL_S;
  uint8_t filter_health_dead_after = DEFAULT_FILTER_HEALTH_DEAD_AFTER;
  int wake_before_bus_s = DEFAULT_WAKE_BEFORE_BUS_S;
  int boot_margin_s = DEFAULT_BOOT_MARGIN_S;
  int active_threshold_s = DEFAULT_ACTIVE_THRESHOLD_S;
  int no_data_sleep_s = DEFAULT_NO_DATA_SLEEP_S;
  int api_failure_retry_s = DEFAULT_API_FAILURE_RETRY_S;
  unsigned long_sleep_for_nightly_clean_s =
      DEFAULT_LONG_SLEEP_FOR_NIGHTLY_CLEAN_S;
  int nightly_deep_clean_interval_s = DEFAULT_NIGHTLY_DEEP_CLEAN_INTERVAL_S;
  unsigned btn_long_press_ms = DEFAULT_BTN_LONG_PRESS_MS;
};

// Bundle of HAL handles + framebuffers + config the cycle functions consume.
// Held only as references; lifetime is the caller's responsibility (typically
// file-scope statics in main.cpp).
struct CycleDeps {
  IClock &clock;
  INetwork &net;
  ISleep &sleep;
  IPersistentStore &store;
  IDisplay &display;
  IRenderer &renderer;
  Frame &curr;
  Frame &prev;
  const CycleConfig &cfg;
};

// Decides whether the next sleep is long enough + last clean is old enough to
// promote the upcoming partial to a full deep clean. Pure data, host-testable.
bool shouldPromoteToNightlyClean(unsigned next_sleep_s, time_t now,
                                 time_t last_deep_clean,
                                 const CycleConfig &cfg);

// Cold-boot path: WiFi+NTP boot sequence, first fetch, deep-clean redraw,
// then sleep.
void runColdCycle(CycleDeps &deps, PersistedMeta &meta);

// Warm-cycle path: fetch + render + plan sleep. The bread-and-butter cycle.
void runWarmCycle(CycleDeps &deps, PersistedMeta &meta);

// Button-wake entry: classify long-vs-short press; long press triggers
// B/W reset, then either path runs runWarmCycle.
void runButtonWake(CycleDeps &deps, IButton &btn, PersistedMeta &meta);

// Active-phase loop body: poll button (handles long-press while awake),
// then run a warm cycle.
void pollButtonAndRunWarm(CycleDeps &deps, IButton &btn, PersistedMeta &meta);

// Long-press action: deep-clean the panel and redraw the last good frame
// (or an empty frame if nothing persisted). Caller still runs a warm cycle
// afterwards to fetch fresh data.
void runBwReset(CycleDeps &deps, PersistedMeta &meta);

} // namespace bustaferl

#endif
