#include <unity.h>

#include "logic/filter_health.h"

using namespace bustaferl;

void test_fresh_is_not_dead() {
  FilterHealth fh(3);
  TEST_ASSERT_FALSE(fh.isDead());
}

void test_three_consecutive_misses_dead() {
  FilterHealth fh(3);
  fh.recordCall(true, false);
  TEST_ASSERT_FALSE(fh.isDead());
  fh.recordCall(true, false);
  TEST_ASSERT_FALSE(fh.isDead());
  fh.recordCall(true, false);
  TEST_ASSERT_TRUE(fh.isDead());
}

void test_match_in_between_resets_streak() {
  FilterHealth fh(3);
  fh.recordCall(true, false);
  fh.recordCall(true, false);
  fh.recordCall(true, true); // reset
  fh.recordCall(true, false);
  TEST_ASSERT_FALSE(fh.isDead());
}

void test_silent_rbl_does_not_increment() {
  FilterHealth fh(3);
  fh.recordCall(true, false);
  fh.recordCall(false, false); // no signal
  fh.recordCall(false, false);
  TEST_ASSERT_FALSE(fh.isDead());
  fh.recordCall(true, false);
  fh.recordCall(true, false);
  TEST_ASSERT_TRUE(fh.isDead());
}

void test_streak_persists_via_setStreak() {
  FilterHealth fh(3);
  fh.setStreak(2);
  fh.recordCall(true, false);
  TEST_ASSERT_TRUE(fh.isDead());
}

void setUp() {}
void tearDown() {}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_fresh_is_not_dead);
  RUN_TEST(test_three_consecutive_misses_dead);
  RUN_TEST(test_match_in_between_resets_streak);
  RUN_TEST(test_silent_rbl_does_not_increment);
  RUN_TEST(test_streak_persists_via_setStreak);
  return UNITY_END();
}
