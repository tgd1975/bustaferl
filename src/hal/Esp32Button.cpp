#include "Esp32Button.h"

#ifndef NATIVE_BUILD

#include <Arduino.h>
#include <driver/rtc_io.h>

namespace bustaferl {

void Esp32Button::init() {
  // deepSleep() latches an RTC-domain pull-up on this pad (rtc_gpio_hold_en) so
  // the button wakes reliably from deep sleep. That hold freezes the pad, so on
  // the way back up we must release it before the digital-domain pinMode takes
  // over — otherwise digitalRead() reads the frozen RTC config, not the live
  // line. Harmless when no hold is active (e.g. after a light-sleep wake).
  const gpio_num_t pad = static_cast<gpio_num_t>(pin_);
  if (rtc_gpio_is_valid_gpio(pad)) {
    rtc_gpio_hold_dis(pad);
  }
  pinMode(pin_, INPUT_PULLUP);
}

bool Esp32Button::isPressed() { return digitalRead(pin_) == LOW; }

std::uint32_t Esp32Button::nowMs() { return millis(); }

void Esp32Button::sleepMs(std::uint32_t ms) { delay(ms); }

} // namespace bustaferl

#endif
