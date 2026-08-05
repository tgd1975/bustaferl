#ifndef BUSTAFERL_ISLEEP_H
#define BUSTAFERL_ISLEEP_H

#include <cstdint>

namespace bustaferl {

enum class WakeCause : std::uint8_t {
  ColdBoot,
  Timer,
  Button, // GPIO 0 (boot button) pulled LOW during sleep
  Other,
};

// The chip's actual reset cause (esp_reset_reason()), distinct from
// WakeCause: a brownout/watchdog/panic reset during warm operation reports
// WakeCause::ColdBoot (see selectCycle()'s routing comment) but RTC memory
// survives. ResetReason is what lets the UI tell the user WHICH kind of
// unplanned reset just happened instead of just "not a deep-sleep wake".
enum class ResetReason : std::uint8_t {
  Normal, // power-on, deep-sleep wake, or a deliberate software reset
  Brownout,
  WatchdogOrPanic,
  Other,
};

class ISleep {
public:
  virtual ~ISleep() = default;
  virtual WakeCause wakeupCause() = 0;
  virtual ResetReason lastResetReason() = 0;
  // Enter deep sleep for `seconds`. On hardware this does not return — the
  // ESP32 reboots from cold. Host fakes do return so the test can inspect
  // post-sleep state; callers must not rely on unreachability.
  virtual void deepSleep(unsigned seconds) = 0;
  // Light sleep that does return; used between polls inside the active phase.
  virtual void lightSleep(unsigned seconds) = 0;
};

} // namespace bustaferl

#endif
