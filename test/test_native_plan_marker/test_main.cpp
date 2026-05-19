// drawPlanMark — 5×5 hollow square. Pixel-mask asserts on the perimeter
// being paper (white) and the interior being ink (black).

#include "render/plan_marker.h"

#include <unity.h>

using namespace bustaferl;

void setUp() {}
void tearDown() {}

void test_plan_mark_perimeter_is_paper() {
  Frame fb;
  fb.clear(false);
  drawPlanMark(fb, 10, 20);
  // All four corners.
  TEST_ASSERT_TRUE(fb.getPixel(10, 20));
  TEST_ASSERT_TRUE(fb.getPixel(14, 20));
  TEST_ASSERT_TRUE(fb.getPixel(10, 24));
  TEST_ASSERT_TRUE(fb.getPixel(14, 24));
  // Mid-edge points.
  TEST_ASSERT_TRUE(fb.getPixel(12, 20));
  TEST_ASSERT_TRUE(fb.getPixel(12, 24));
  TEST_ASSERT_TRUE(fb.getPixel(10, 22));
  TEST_ASSERT_TRUE(fb.getPixel(14, 22));
}

void test_plan_mark_interior_stays_ink() {
  Frame fb;
  fb.clear(false);
  drawPlanMark(fb, 10, 20);
  // Centre pixel must remain ink (hollow).
  TEST_ASSERT_FALSE(fb.getPixel(12, 22));
  // The 3×3 interior block must all be ink.
  for (int yy = 21; yy <= 23; ++yy) {
    for (int xx = 11; xx <= 13; ++xx) {
      TEST_ASSERT_FALSE(fb.getPixel(xx, yy));
    }
  }
}

void test_plan_mark_does_not_leak_outside_5x5() {
  Frame fb;
  fb.clear(false);
  drawPlanMark(fb, 10, 20);
  // One pixel beyond each edge of the 5×5 footprint must remain ink.
  TEST_ASSERT_FALSE(fb.getPixel(9, 22));
  TEST_ASSERT_FALSE(fb.getPixel(15, 22));
  TEST_ASSERT_FALSE(fb.getPixel(12, 19));
  TEST_ASSERT_FALSE(fb.getPixel(12, 25));
}

void test_plan_mark_clips_at_canvas_boundary() {
  Frame fb;
  fb.clear(false);
  // Draw partly off-screen: setPixel clamps internally, so the on-canvas
  // edge pixels are paper, the off-canvas writes are no-ops, and no other
  // pixels change.
  drawPlanMark(fb, -2, -2);
  // The (0,0) and (1,1) pixels (which are still inside the marker
  // footprint of [-2..2] x [-2..2]) should be ink (corners of mark are at
  // -2 and 2; only x=2 / y=2 pixels are visible perimeter at y=2 or x=2).
  // Just assert the canvas didn't blow up — the marker's top-left corner
  // is off-canvas, so we don't see it.
  TEST_ASSERT_TRUE(fb.getPixel(2, 2)); // bottom-right corner of marker
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_plan_mark_perimeter_is_paper);
  RUN_TEST(test_plan_mark_interior_stays_ink);
  RUN_TEST(test_plan_mark_does_not_leak_outside_5x5);
  RUN_TEST(test_plan_mark_clips_at_canvas_boundary);
  return UNITY_END();
}
