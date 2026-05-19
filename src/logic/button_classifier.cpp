#include "logic/button_classifier.h"

namespace bustaferl {

ButtonPress classifyHeld(IButton &btn, std::uint32_t long_press_ms) {
  btn.init();
  btn.sleepMs(BUTTON_DEBOUNCE_MS); // debounce settle
  if (!btn.isPressed()) {
    return ButtonPress::Short;
  }
  const std::uint32_t t0 = btn.nowMs();
  bool long_latched = false;
  while (btn.isPressed()) {
    if (!long_latched && btn.nowMs() - t0 >= long_press_ms) {
      long_latched = true;
    }
    btn.sleepMs(BUTTON_POLL_MS);
  }
  return long_latched ? ButtonPress::Long : ButtonPress::Short;
}

} // namespace bustaferl
