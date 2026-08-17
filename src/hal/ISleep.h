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
  // Wait `seconds`, returning early if the boot button is pressed. The only
  // wait the cycle has: used between active-phase polls and to hold a screen
  // on display.
  //
  // Deliberately not a light sleep. esp_light_sleep_start() arms the RTC
  // watchdog around its own exit path, and that exit fails on roughly 1.5% of
  // sleeps — a known upstream ESP32 defect, not ours. The watchdog recovers
  // the chip, but each recovery is a reset: boot screen, full refresh, lost
  // active phase, once per ~25 min of active phase (soak 2026-08-16).
  //
  // Nothing was given up for that. The device runs on USB
  // (docs/HARDWARE.md), and during service hours it never deep-sleeps —
  // planSleep() keeps returning Active because the next departure is always
  // close — so light sleep was the entire daytime power state and on a mains
  // supply it bought nothing. Deep sleep is untouched and still runs the
  // overnight no-data path, which is where the saving actually is.
  virtual void pause(unsigned seconds) = 0;
};

} // namespace bustaferl

#endif
