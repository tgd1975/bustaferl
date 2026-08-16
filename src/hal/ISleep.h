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
  // Light sleep that does return. No longer used by the cycle — see pause() —
  // but kept as the real thing: test_device_sleep drives it to verify the
  // wakeupCause() mapping (a light-sleep timer wake reports the same enum as
  // a deep-sleep one), and a future battery build would want it back.
  virtual void lightSleep(unsigned seconds) = 0;

  // Wait `seconds`, returning early if the boot button is pressed. This is
  // what the cycle uses between polls and to hold a screen on display.
  //
  // Deliberately NOT a light sleep. On the ESP32, esp_light_sleep_start()
  // arms the RTC watchdog around its own exit path, and that exit fails on
  // roughly 2% of sleeps — a known upstream defect, not ours. The watchdog
  // recovers the chip, but each recovery is a reset: boot screen, full
  // refresh, lost active phase. Measured at ~1 reset per 25 min of active
  // phase (soak 2026-08-16, 3 occurrences across ~110 sleeps).
  //
  // The device runs on USB (docs/HARDWARE.md), and during service hours it
  // never deep-sleeps at all — planSleep() keeps returning Active because the
  // next departure is always close, so light sleep was the whole daytime
  // power state. On a mains supply it bought nothing and cost a reset every
  // ~25 min, so the cycle just stays awake instead. Deep sleep is untouched:
  // it still runs on the overnight no-data path and is where the power
  // savings actually matter.
  //
  // Defaults to lightSleep() so host fakes keep their existing behaviour and
  // recording; Esp32Sleep overrides it with a plain wait.
  virtual void pause(unsigned seconds) { lightSleep(seconds); }
};

} // namespace bustaferl

#endif
