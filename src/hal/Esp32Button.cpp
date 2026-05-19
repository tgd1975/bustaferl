#include "Esp32Button.h"

#ifndef NATIVE_BUILD

#include <Arduino.h>

namespace bustaferl {

void Esp32Button::init() { pinMode(pin_, INPUT_PULLUP); }

bool Esp32Button::isPressed() { return digitalRead(pin_) == LOW; }

std::uint32_t Esp32Button::nowMs() { return millis(); }

void Esp32Button::sleepMs(std::uint32_t ms) { delay(ms); }

} // namespace bustaferl

#endif
