#ifndef BUSTAFERL_ESP32BUTTON_H
#define BUSTAFERL_ESP32BUTTON_H

#ifndef NATIVE_BUILD

#include "IButton.h"

namespace bustaferl {

class Esp32Button : public IButton {
public:
  explicit Esp32Button(int pin) : pin_(pin) {}
  void init() override;
  bool isPressed() override;
  std::uint32_t nowMs() override;
  void sleepMs(std::uint32_t ms) override;

private:
  int pin_;
};

} // namespace bustaferl

#endif
#endif
