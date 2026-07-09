// Tier 1 — diagnostic pager navigation (logic/diag_mode). Forward-with-wrap,
// long-press exit, timeout exit. The device-side loop (light-sleep + button +
// render) is exercised via cycle_runner recording fakes.

#include "logic/diag_mode.h"

#include <unity.h>

using namespace bustaferl;

void setUp() {}
void tearDown() {}

void test_short_advances_one_page() {
  DiagStep s = diagNext(0, ButtonPress::Short, /*timed_out=*/false);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(DiagAction::ShowPage),
                        static_cast<int>(s.action));
  TEST_ASSERT_EQUAL_INT(1, s.page);
}

void test_short_wraps_after_last_page() {
  DiagStep s =
      diagNext(DIAG_PAGE_COUNT - 1, ButtonPress::Short, /*timed_out=*/false);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(DiagAction::ShowPage),
                        static_cast<int>(s.action));
  TEST_ASSERT_EQUAL_INT(0, s.page);
}

void test_long_exits() {
  DiagStep s = diagNext(2, ButtonPress::Long, /*timed_out=*/false);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(DiagAction::Exit),
                        static_cast<int>(s.action));
}

void test_timeout_exits_even_with_short() {
  DiagStep s = diagNext(1, ButtonPress::Short, /*timed_out=*/true);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(DiagAction::Exit),
                        static_cast<int>(s.action));
}

void test_none_keeps_page() {
  DiagStep s = diagNext(2, ButtonPress::None, /*timed_out=*/false);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(DiagAction::ShowPage),
                        static_cast<int>(s.action));
  TEST_ASSERT_EQUAL_INT(2, s.page);
}

void test_double_advances_like_short() {
  DiagStep s = diagNext(0, ButtonPress::Double, /*timed_out=*/false);
  TEST_ASSERT_EQUAL_INT(1, s.page);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_short_advances_one_page);
  RUN_TEST(test_short_wraps_after_last_page);
  RUN_TEST(test_long_exits);
  RUN_TEST(test_timeout_exits_even_with_short);
  RUN_TEST(test_none_keeps_page);
  RUN_TEST(test_double_advances_like_short);
  return UNITY_END();
}
