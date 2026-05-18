#ifndef BUSTAFERL_SLEEP_PLANNER_H
#define BUSTAFERL_SLEEP_PLANNER_H

#include "../data/StreamSnapshot.h"

#include <cstdint>
#include <ctime>

namespace bustaferl {

enum class Mode : std::uint8_t {
  DeepSleep,
  Active,
};

struct SleepDecision {
  Mode mode = Mode::Active;
  unsigned seconds = 0; // only meaningful if mode == DeepSleep
};

// Defaults — overridable per call site, but these are the production values
// derived from CONCEPT.md §6.
constexpr int DEFAULT_WAKE_BEFORE_BUS_S = 900;  // 15 min head start
constexpr int DEFAULT_BOOT_MARGIN_S = 30;       // cold-boot + NTP + fetch
constexpr int DEFAULT_ACTIVE_THRESHOLD_S = 120; // stay active if next bus <
constexpr int DEFAULT_NO_DATA_SLEEP_S = 1800;   // API ok but no departures
constexpr int DEFAULT_API_FAILURE_RETRY_S = 60; // !snap.api_ok → short retry

struct SleepConfig {
  int wake_before_bus_s = DEFAULT_WAKE_BEFORE_BUS_S;
  int boot_margin_s = DEFAULT_BOOT_MARGIN_S;
  int active_threshold_s = DEFAULT_ACTIVE_THRESHOLD_S;
  int no_data_sleep_s = DEFAULT_NO_DATA_SLEEP_S;
  int api_failure_retry_s = DEFAULT_API_FAILURE_RETRY_S;
};

// Decides whether to enter deep sleep based on the earliest departure across
// all streams. See CONCEPT.md §6.
SleepDecision planSleep(const StreamSnapshot &snap, time_t now,
                        const SleepConfig &cfg);

// True if a deep clean is overdue: never cleaned (last == 0) or at least
// `min_interval_s` has elapsed. Guards against a clock that hasn't caught up
// yet (now < last_deep_clean) by returning false in that case.
bool needsNightlyDeepClean(time_t now, time_t last_deep_clean,
                           int min_interval_s);

} // namespace bustaferl

#endif
