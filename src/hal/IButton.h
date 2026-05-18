#ifndef BUSTAFERL_IBUTTON_H
#define BUSTAFERL_IBUTTON_H

#include <cstdint>

namespace bustaferl {

// Minimal HAL abstraction for the boot button. classify_held in
// logic/button_classifier reads `isPressed()` in a debounce-and-poll loop
// and calls `sleepMs` to yield between samples. `nowMs()` is the monotonic
// time source the long-press latch checks against (millis() on ESP32,
// fake-clock-driven counter on host tests).
class IButton {
public:
  virtual ~IButton() = default;
  // Configure the underlying GPIO (called once at the start of a
  // classification round; idempotent). On ESP32 maps to
  // pinMode(pin, INPUT_PULLUP).
  virtual void init() = 0;
  // True iff the button is currently pulled LOW (i.e. held).
  virtual bool isPressed() = 0;
  // Monotonic millisecond counter for the long-press latch.
  virtual std::uint32_t nowMs() = 0;
  // Cooperative yield between polls — millisecond resolution. Production
  // calls Arduino's delay(); host fakes advance the FakeButton clock.
  virtual void sleepMs(std::uint32_t ms) = 0;
};

} // namespace bustaferl

#endif
