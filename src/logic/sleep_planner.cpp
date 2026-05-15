#include "sleep_planner.h"

#include <climits>

namespace bustaferl {

SleepDecision planSleep(const StreamSnapshot &snap, time_t now,
                        const SleepConfig &cfg) {
  time_t t_ref = 0;
  bool have = false;
  for (int s = 0; s < STREAM_COUNT; ++s) {
    for (int k = 0; k < SLOTS_PER_STREAM; ++k) {
      const auto &d = snap.stream[s].slot[k];
      if (!d.valid)
        continue;
      if (!have || d.when < t_ref) {
        t_ref = d.when;
        have = true;
      }
    }
  }

  if (!have) {
    return SleepDecision{Mode::DeepSleep,
                         static_cast<unsigned>(cfg.no_data_sleep_s)};
  }

  long wake_at =
      static_cast<long>(t_ref) - cfg.wake_before_bus_s - cfg.boot_margin_s;
  long delta = wake_at - static_cast<long>(now);

  if (delta >= cfg.active_threshold_s) {
    return SleepDecision{Mode::DeepSleep, static_cast<unsigned>(delta)};
  }
  return SleepDecision{Mode::Active, 0};
}

} // namespace bustaferl
