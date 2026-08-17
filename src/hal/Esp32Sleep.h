#ifndef BUSTAFERL_ESP32SLEEP_H
#define BUSTAFERL_ESP32SLEEP_H

#ifndef NATIVE_BUILD

#include "ISleep.h"

namespace bustaferl {

class Esp32Sleep : public ISleep {
public:
  WakeCause wakeupCause() override;
  ResetReason lastResetReason() override;
  [[noreturn]] void deepSleep(unsigned seconds) override;
  void pause(unsigned seconds) override;

  // Real light sleep — deliberately NOT part of ISleep, because the cycle
  // must not use it (see ISleep::pause()). It survives solely as
  // test_device_sleep's proxy for the deep-sleep wake path: a timer wake out
  // of light sleep reports the same wakeupCause() enum as one out of deep
  // sleep, but without the chip reset that would kill the Unity process.
  static void lightSleep(unsigned seconds);
};

} // namespace bustaferl

#endif
#endif
