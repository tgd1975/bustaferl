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
    // Differentiate "API responded with no buses" (overnight, sleep long)
    // from "API/network failed" (short retry — do not back off for half an
    // hour just because the upstream blipped).
    unsigned secs = snap.api_ok
                        ? static_cast<unsigned>(cfg.no_data_sleep_s)
                        : static_cast<unsigned>(cfg.api_failure_retry_s);
    return SleepDecision{Mode::DeepSleep, secs};
  }

  long wake_at =
      static_cast<long>(t_ref) - cfg.wake_before_bus_s - cfg.boot_margin_s;
  long delta = wake_at - static_cast<long>(now);

  if (delta >= cfg.active_threshold_s) {
    return SleepDecision{Mode::DeepSleep, static_cast<unsigned>(delta)};
  }
  return SleepDecision{Mode::Active, 0};
}

bool needsNightlyDeepClean(time_t now, time_t last_deep_clean,
                           int min_interval_s) {
  if (last_deep_clean == 0)
    return true;
  if (now < last_deep_clean)
    return false;
  return (now - last_deep_clean) >= min_interval_s;
}

} // namespace bustaferl
