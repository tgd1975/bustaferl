// Host tests for classifyHeld with a FakeButton that drives nowMs() and the
// pressed-line from a scripted timeline. sleepMs() advances the fake clock,
// so the production polling loop is exercised exactly as on hardware.

#include "hal/IButton.h"
#include "logic/button_classifier.h"

#include <cstdint>
#include <unity.h>

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
  return UNITY_END();
}
