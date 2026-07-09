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

ButtonPress classifyPress(IButton &btn, std::uint32_t long_press_ms,
                          std::uint32_t double_click_ms) {
  const ButtonPress first = classifyHeld(btn, long_press_ms);
  if (first != ButtonPress::Short) {
    return first; // Long (or None) is decisive — no double-click concept.
  }
  // Short: watch for a second press starting within the window. A second
  // press upgrades to Double; consume its release so the caller resumes on a
  // released line.
  const std::uint32_t t0 = btn.nowMs();
  while (btn.nowMs() - t0 < double_click_ms) {
    if (btn.isPressed()) {
      classifyHeld(btn, long_press_ms); // drain the second press
      return ButtonPress::Double;
    }
    btn.sleepMs(BUTTON_POLL_MS);
  }
  return ButtonPress::Short;
}

} // namespace bustaferl
