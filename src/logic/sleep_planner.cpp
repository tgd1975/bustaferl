#include "sleep_planner.h"

#include <climits>

namespace bustaferl {

SleepDecision planSleep(const StreamSnapshot &snap, time_t now,
                        const SleepConfig &cfg) {
  // Failed fetch → short retry, unconditionally. On a transient WiFi/API
  // blip the merged view still carries schedule HINTS (mergeSlots fills
  // empty realtime slots from the EFA timetable), so a valid slot may point
  // hours ahead — planning the wake against it put the device into a
  // multi-hour deep sleep after a single failed cycle ("display froze").
  // Never commit to a long sleep based on a cycle that couldn't reach the
  // API; retry shortly instead and let a successful fetch plan the real one.
  if (!snap.api_ok) {
    return SleepDecision{Mode::DeepSleep,
                         static_cast<unsigned>(cfg.api_failure_retry_s)};
  }

  time_t t_ref = 0;
  bool have = false;
  for (const auto &stream : snap.stream) {
    for (const auto &d : stream.slot) {
      if (!d.valid)
        continue;
      if (!have || d.when < t_ref) {
        t_ref = d.when;
        have = true;
      }
    }
  }

  if (!have) {
    // API responded but no departures anywhere (overnight): sleep long.
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

bool needsNightlyDeepClean(time_t now, time_t last_deep_clean,
                           int min_interval_s) {
  if (last_deep_clean == 0)
    return true;
  if (now < last_deep_clean)
    return false;
  return (now - last_deep_clean) >= min_interval_s;
}

} // namespace bustaferl
