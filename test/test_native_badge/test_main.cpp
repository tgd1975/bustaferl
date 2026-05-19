// Badge geometry primitive — pixel-stamp asserts on the paper rectangle
// of each badge size. Goes through HostCanvas so the test exercises the
// same drawBadge() the production renderer uses.

#include "render/badge.h"
#include "render/canvas_host.h"

#include <unity.h>

using namespace bustaferl;

void setUp() {}
void tearDown() {}

void test_drawBadge_sm_fills_22x14_paper_box() {
  Frame fb;
  fb.clear(false);
  render::HostCanvas canvas(fb);
  int right = drawBadge(canvas, 18, 200, "S2", BadgeSize::Sm);
  TEST_ASSERT_EQUAL_INT(18 + 22, right);
  // Centre pixel of the badge rectangle must be paper (white). Text may
  // overwrite some interior pixels back to ink, but the corners are
  // guaranteed paper because the 5×7 glyph doesn't reach that far.
  TEST_ASSERT_TRUE(fb.getPixel(18 + 1, 200 + 1));
  TEST_ASSERT_FALSE(fb.getPixel(17, 200 + 7));
  TEST_ASSERT_FALSE(fb.getPixel(18 + 22, 200 + 7));
  TEST_ASSERT_FALSE(fb.getPixel(18 + 11, 199));
  TEST_ASSERT_FALSE(fb.getPixel(18 + 11, 200 + 14));
}

void test_drawBadge_md_fills_28x18() {
  Frame fb;
  fb.clear(false);
  render::HostCanvas canvas(fb);
  int right = drawBadge(canvas, 18, 100, "58B", BadgeSize::Md);
  TEST_ASSERT_EQUAL_INT(18 + 28, right);
  TEST_ASSERT_TRUE(fb.getPixel(18 + 1, 100 + 1));
  TEST_ASSERT_FALSE(fb.getPixel(18 + 28, 100 + 9));
}

void test_drawBadge_lg_fills_36x22() {
  Frame fb;
  fb.clear(false);
  render::HostCanvas canvas(fb);
  int right = drawBadge(canvas, 18, 32, "58A", BadgeSize::Lg);
  TEST_ASSERT_EQUAL_INT(18 + 36, right);
  TEST_ASSERT_TRUE(fb.getPixel(18 + 1, 32 + 1));
  TEST_ASSERT_FALSE(fb.getPixel(18 + 36, 32 + 11));
}

void test_drawBadge_does_not_touch_unrelated_pixels() {
  Frame fb;
  fb.clear(false);
  fb.setPixel(300, 250, true); // sentinel paper pixel far from the badge
  render::HostCanvas canvas(fb);
  drawBadge(canvas, 18, 32, "58A", BadgeSize::Lg);
  TEST_ASSERT_TRUE(fb.getPixel(300, 250));
}

void test_badgeBounds_table_matches_design_handoff() {
  TEST_ASSERT_EQUAL_INT(22, badgeBounds(BadgeSize::Sm).w);
  TEST_ASSERT_EQUAL_INT(14, badgeBounds(BadgeSize::Sm).h);
  TEST_ASSERT_EQUAL_INT(28, badgeBounds(BadgeSize::Md).w);
  TEST_ASSERT_EQUAL_INT(18, badgeBounds(BadgeSize::Md).h);
  TEST_ASSERT_EQUAL_INT(36, badgeBounds(BadgeSize::Lg).w);
  TEST_ASSERT_EQUAL_INT(22, badgeBounds(BadgeSize::Lg).h);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_drawBadge_sm_fills_22x14_paper_box);
  RUN_TEST(test_drawBadge_md_fills_28x18);
  RUN_TEST(test_drawBadge_lg_fills_36x22);
  RUN_TEST(test_drawBadge_does_not_touch_unrelated_pixels);
  RUN_TEST(test_badgeBounds_table_matches_design_handoff);
  return UNITY_END();
}
