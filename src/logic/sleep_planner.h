#ifndef BUSTAFERL_SLEEP_PLANNER_H
#define BUSTAFERL_SLEEP_PLANNER_H

#include <ctime>

#include "../data/StreamSnapshot.h"

namespace bustaferl {

enum class Mode {
  DeepSleep,
  Active,
};

struct SleepDecision {
  Mode mode = Mode::Active;
  unsigned seconds = 0; // only meaningful if mode == DeepSleep
};

struct SleepConfig {
  int wake_before_bus_s = 900;
  int boot_margin_s = 30;
  int active_threshold_s = 120;
  int no_data_sleep_s = 1800;     // API ok but no departures (overnight)
  int api_failure_retry_s = 60;   // !snap.api_ok → short retry
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
