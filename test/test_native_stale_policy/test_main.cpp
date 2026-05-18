#include "logic/stale_policy.h"

#include <unity.h>

using namespace bustaferl;

void test_fresh_is_not_stale() { TEST_ASSERT_FALSE(isStale(1000, 1010, 180)); }

void test_exactly_under_threshold_is_not_stale() {
  TEST_ASSERT_FALSE(isStale(1000, 1000 + 179, 180));
}

void test_at_threshold_is_stale() {
  TEST_ASSERT_TRUE(isStale(1000, 1000 + 180, 180));
}

void test_over_threshold_is_stale() {
  TEST_ASSERT_TRUE(isStale(1000, 1000 + 181, 180));
}

void test_never_synced_is_stale() { TEST_ASSERT_TRUE(isStale(0, 5000, 180)); }

void test_clock_running_backwards_is_not_stale() {
  TEST_ASSERT_FALSE(isStale(2000, 1000, 180));
}

void setUp() {}
void tearDown() {}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_fresh_is_not_stale);
  RUN_TEST(test_exactly_under_threshold_is_not_stale);
  RUN_TEST(test_at_threshold_is_stale);
  RUN_TEST(test_over_threshold_is_stale);
  RUN_TEST(test_never_synced_is_stale);
  RUN_TEST(test_clock_running_backwards_is_not_stale);
  return UNITY_END();
}
