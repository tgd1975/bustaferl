// Badge geometry primitive — pixel-stamp asserts on the three badge sizes.
// Text rendering goes through U8g2 (ESP32-only); this bucket exercises only
// the geometry that lives in drawBadgeGeometry().

#include "render/badge.h"

#include <unity.h>

using namespace bustaferl;

void setUp() {}
void tearDown() {}

void test_drawBadgeGeometry_sm_fills_22x14() {
  Frame fb;
  fb.clear(false); // start black
  int right = drawBadgeGeometry(fb, 18, 200, BadgeSize::Sm);
  TEST_ASSERT_EQUAL_INT(18 + 22, right);
  // Centre pixel of the badge must be paper (white).
  TEST_ASSERT_TRUE(fb.getPixel(18 + 11, 200 + 7));
  // One pixel left of the badge must still be ink (black).
  TEST_ASSERT_FALSE(fb.getPixel(17, 200 + 7));
  // One pixel right of the badge must still be ink.
  TEST_ASSERT_FALSE(fb.getPixel(18 + 22, 200 + 7));
  // One pixel above must still be ink.
  TEST_ASSERT_FALSE(fb.getPixel(18 + 11, 199));
  // One pixel below must still be ink.
  TEST_ASSERT_FALSE(fb.getPixel(18 + 11, 200 + 14));
}

void test_drawBadgeGeometry_md_fills_28x18() {
  Frame fb;
  fb.clear(false);
  int right = drawBadgeGeometry(fb, 18, 100, BadgeSize::Md);
  TEST_ASSERT_EQUAL_INT(18 + 28, right);
  TEST_ASSERT_TRUE(fb.getPixel(18 + 14, 100 + 9));
  TEST_ASSERT_FALSE(fb.getPixel(18 + 28, 100 + 9));
}

void test_drawBadgeGeometry_lg_fills_36x22() {
  Frame fb;
  fb.clear(false);
  int right = drawBadgeGeometry(fb, 18, 32, BadgeSize::Lg);
  TEST_ASSERT_EQUAL_INT(18 + 36, right);
  TEST_ASSERT_TRUE(fb.getPixel(18 + 18, 32 + 11));
  TEST_ASSERT_FALSE(fb.getPixel(18 + 36, 32 + 11));
}

void test_drawBadgeGeometry_does_not_touch_unrelated_pixels() {
  Frame fb;
  fb.clear(false);
  // Drop a sentinel paper pixel far away from the badge area.
  fb.setPixel(300, 250, true);
  drawBadgeGeometry(fb, 18, 32, BadgeSize::Lg);
  TEST_ASSERT_TRUE(fb.getPixel(300, 250));
}

void test_badgeBounds_table_matches_design_handoff() {
  // Sanity-check the constexpr table against the design handoff numbers
  // so a future tweak doesn't silently rescale every badge.
  TEST_ASSERT_EQUAL_INT(22, badgeBounds(BadgeSize::Sm).w);
  TEST_ASSERT_EQUAL_INT(14, badgeBounds(BadgeSize::Sm).h);
  TEST_ASSERT_EQUAL_INT(28, badgeBounds(BadgeSize::Md).w);
  TEST_ASSERT_EQUAL_INT(18, badgeBounds(BadgeSize::Md).h);
  TEST_ASSERT_EQUAL_INT(36, badgeBounds(BadgeSize::Lg).w);
  TEST_ASSERT_EQUAL_INT(22, badgeBounds(BadgeSize::Lg).h);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_drawBadgeGeometry_sm_fills_22x14);
  RUN_TEST(test_drawBadgeGeometry_md_fills_28x18);
  RUN_TEST(test_drawBadgeGeometry_lg_fills_36x22);
  RUN_TEST(test_drawBadgeGeometry_does_not_touch_unrelated_pixels);
  RUN_TEST(test_badgeBounds_table_matches_design_handoff);
  return UNITY_END();
}
