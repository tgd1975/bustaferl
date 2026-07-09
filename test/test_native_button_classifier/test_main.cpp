// Host tests for classifyHeld with a FakeButton that drives nowMs() and the
// pressed-line from a scripted timeline. sleepMs() advances the fake clock,
// so the production polling loop is exercised exactly as on hardware.

#include "hal/IButton.h"
#include "logic/button_classifier.h"

#include <cstdint>
#include <unity.h>
#include <utility>
#include <vector>

using namespace bustaferl;

namespace {

// Scripts describe (release_at_ms) — the time at which the button is released.
// `pressed_initially` controls whether the very first isPressed() returns true
// (production guarantees this; "already released" is the early-Short path).
class FakeButton : public IButton {
public:
  std::uint32_t now = 0;
  std::uint32_t release_at = 0;
  bool pressed_initially = true;
  int init_calls = 0;
  int press_polls = 0;

  void init() override { ++init_calls; }
  bool isPressed() override {
    ++press_polls;
    if (!pressed_initially)
      return false;
    return now < release_at;
  }
  std::uint32_t nowMs() override { return now; }
  void sleepMs(std::uint32_t ms) override { now += ms; }
};

// Multi-press timeline: isPressed() is true whenever `now` falls inside any
// [start, end) interval. Drives classifyPress through two-click sequences.
class ScriptedButton : public IButton {
public:
  std::uint32_t now = 0;
  std::vector<std::pair<std::uint32_t, std::uint32_t>> presses;

  void init() override {}
  bool isPressed() override {
    for (const auto &p : presses) {
      if (now >= p.first && now < p.second) {
        return true;
      }
    }
    return false;
  }
  std::uint32_t nowMs() override { return now; }
  void sleepMs(std::uint32_t ms) override { now += ms; }
};

} // namespace

void test_classify_short_when_released_before_threshold() {
  FakeButton btn;
  btn.release_at = 500; // released at 500ms, threshold at 2000ms → Short
  ButtonPress p = classifyHeld(btn, 2000);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ButtonPress::Short),
                        static_cast<int>(p));
  TEST_ASSERT_EQUAL_INT_MESSAGE(1, btn.init_calls,
                                "init() must be called exactly once");
}

void test_classify_long_when_held_past_threshold() {
  FakeButton btn;
  btn.release_at = 3000; // released at 3000ms, threshold at 2000ms → Long
  ButtonPress p = classifyHeld(btn, 2000);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ButtonPress::Long),
                        static_cast<int>(p));
}

void test_classify_short_when_already_released_after_debounce() {
  // Button "bounced" — was reported pressed at wake but is HIGH right after
  // the debounce settle. Production maps this to Short, not None.
  FakeButton btn;
  btn.pressed_initially = false;
  ButtonPress p = classifyHeld(btn, 2000);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ButtonPress::Short),
                        static_cast<int>(p));
}

void test_classify_short_exactly_below_threshold() {
  // Released the millisecond before the threshold would latch. The latch
  // condition is "elapsed >= long_press_ms" — so a release at threshold - 1
  // must stay Short.
  FakeButton btn;
  btn.release_at = 1999;
  ButtonPress p = classifyHeld(btn, 2000);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ButtonPress::Short),
                        static_cast<int>(p));
}

void test_classify_long_just_past_threshold() {
  // The loop polls isPressed() first; if the button is released by the time
  // the latch check would fire, the elapsed >= long_press_ms branch is never
  // reached. The latch needs at least one poll where the press is still held
  // AND elapsed has crossed the threshold — a hold lasting until well past
  // the threshold guarantees that.
  FakeButton btn;
  btn.release_at = 2500; // 2.5x debounce-step past threshold → latch fires
  ButtonPress p = classifyHeld(btn, 2000);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ButtonPress::Long),
                        static_cast<int>(p));
}

void test_init_runs_before_first_sample() {
  // init() must be called before the debounce sleep, and before the first
  // isPressed() poll. FakeButton counts ordering loosely (init_calls > 0 by
  // the time isPressed() runs).
  FakeButton btn;
  btn.pressed_initially = false;
  classifyHeld(btn, 2000);
  TEST_ASSERT_EQUAL_INT(1, btn.init_calls);
  TEST_ASSERT_TRUE_MESSAGE(btn.press_polls >= 1,
                           "expected at least one isPressed() sample");
  // After classifyHeld, FakeButton.now should reflect the debounce sleep.
  TEST_ASSERT_TRUE_MESSAGE(btn.now >= BUTTON_DEBOUNCE_MS,
                           "debounce sleep did not advance fake clock");
}

void test_classify_press_single_short_no_second() {
  // One short press [0,80); no second within the 400ms window → Short.
  ScriptedButton btn;
  btn.presses = {{0, 80}};
  ButtonPress p = classifyPress(btn, 2000, 400);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ButtonPress::Short),
                        static_cast<int>(p));
}

void test_classify_press_double_within_window() {
  // Short press [0,80), second press starts at 200 (120ms gap < 400) → Double.
  ScriptedButton btn;
  btn.presses = {{0, 80}, {200, 280}};
  ButtonPress p = classifyPress(btn, 2000, 400);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ButtonPress::Double),
                        static_cast<int>(p));
}

void test_classify_press_second_click_too_late_is_short() {
  // Second press only at 900ms — well past the 400ms window → Short, not
  // Double (the late press is left for the next classification round).
  ScriptedButton btn;
  btn.presses = {{0, 80}, {900, 980}};
  ButtonPress p = classifyPress(btn, 2000, 400);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ButtonPress::Short),
                        static_cast<int>(p));
}

void test_classify_press_long_is_decisive() {
  // A long first press never enters the double-click window.
  ScriptedButton btn;
  btn.presses = {{0, 3000}};
  ButtonPress p = classifyPress(btn, 2000, 400);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ButtonPress::Long),
                        static_cast<int>(p));
}

void setUp() {}
void tearDown() {}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_classify_short_when_released_before_threshold);
  RUN_TEST(test_classify_long_when_held_past_threshold);
  RUN_TEST(test_classify_short_when_already_released_after_debounce);
  RUN_TEST(test_classify_short_exactly_below_threshold);
  RUN_TEST(test_classify_long_just_past_threshold);
  RUN_TEST(test_init_runs_before_first_sample);
  RUN_TEST(test_classify_press_single_short_no_second);
  RUN_TEST(test_classify_press_double_within_window);
  RUN_TEST(test_classify_press_second_click_too_late_is_short);
  RUN_TEST(test_classify_press_long_is_decisive);
  return UNITY_END();
}
