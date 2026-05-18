#ifndef BUSTAFERL_BUTTON_CLASSIFIER_H
#define BUSTAFERL_BUTTON_CLASSIFIER_H

#include "../hal/IButton.h"

#include <cstdint>

namespace bustaferl {

// Boot-button press classification. Mirrors the production semantics one-to-one
// (long-press latches at long_press_ms even if the user holds further; short is
// the default for any release before that point).
enum class ButtonPress : std::uint8_t { None, Short, Long };

// Default debounce settle time after init() before sampling the line — long
// enough to filter switch bounce, short enough to feel responsive.
constexpr std::uint32_t BUTTON_DEBOUNCE_MS = 20;
// Sample period inside the hold loop.
constexpr std::uint32_t BUTTON_POLL_MS = 10;

// Block until the button is released; classify as Short or Long based on
// `long_press_ms`. Assumes the button is currently held (we got here because
// of an EXT0/GPIO wake or a positive poll). If the line is already released
// when we look (debounce has settled and isPressed() == false), treat as a
// transient press → Short.
//
// Pure HAL-driven; no Arduino.h dependency, fully host-testable via a
// FakeButton that drives `nowMs()` / `isPressed()` from a scripted timeline.
ButtonPress classifyHeld(IButton &btn, std::uint32_t long_press_ms);

} // namespace bustaferl

#endif
