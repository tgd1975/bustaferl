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
  int no_data_sleep_s = 1800;
};

// Decides whether to enter deep sleep based on the earliest departure across
// all streams. See CONCEPT.md §6.
SleepDecision planSleep(const StreamSnapshot &snap, time_t now,
                        const SleepConfig &cfg);

} // namespace bustaferl

#endif
