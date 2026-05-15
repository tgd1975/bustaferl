#ifndef BUSTAFERL_ESP32SLEEP_H
#define BUSTAFERL_ESP32SLEEP_H

#ifndef NATIVE_BUILD

#include "ISleep.h"

namespace bustaferl {

class Esp32Sleep : public ISleep {
public:
  WakeCause wakeupCause() override;
  [[noreturn]] void deepSleep(unsigned seconds) override;
  void lightSleep(unsigned seconds) override;
};

} // namespace bustaferl

#endif
#endif
