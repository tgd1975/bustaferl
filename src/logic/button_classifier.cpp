#include "logic/button_classifier.h"

namespace bustaferl {

ButtonPress classifyHeld(IButton &btn, std::uint32_t long_press_ms) {
  btn.init();
  btn.sleepMs(BUTTON_DEBOUNCE_MS); // debounce settle
  if (!btn.isPressed()) {
    return ButtonPress::Short;
  }
  const std::uint32_t t0 = btn.nowMs();
  // Long press fires AT the threshold while the button is still held — the
  // user wanted the action to trigger on the timeout, not on the release after
  // it (waiting for release made a 3 s hold feel like it "did nothing" until
  // let go). We return Long the instant elapsed crosses long_press_ms; the
  // caller then drains the still-held line (waitForRelease) before re-arming a
  // wake source, so the held-LOW GPIO0 doesn't immediately re-trigger.
  while (btn.isPressed()) {
    if (btn.nowMs() - t0 >= long_press_ms) {
      return ButtonPress::Long;
    }
    btn.sleepMs(BUTTON_POLL_MS);
  }
  // Released before the threshold → Short.
  return ButtonPress::Short;
}

void waitForRelease(IButton &btn) {
  while (btn.isPressed()) {
    btn.sleepMs(BUTTON_POLL_MS);
  }
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
