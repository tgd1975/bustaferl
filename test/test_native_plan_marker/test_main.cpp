// drawPlanMark — 5×5 hollow ring (rounded "°" marker). Pixel-mask asserts
// on the rounded edges being paper (white), the four corners + interior
// being ink (black). Exercises the production code path via HostCanvas
// (same Canvas interface as the device).

#include "render/canvas_host.h"
#include "render/plan_marker.h"

#include <unity.h>

using namespace bustaferl;

void setUp() {}
void tearDown() {}

void test_plan_mark_rounded_edges_are_paper() {
  Frame fb;
  fb.clear(false);
  render::HostCanvas canvas(fb);
  drawPlanMark(canvas, 10, 20);
  // Top and bottom edge pixels (excluding the rounded corners).
  TEST_ASSERT_TRUE(fb.getPixel(11, 20));
  TEST_ASSERT_TRUE(fb.getPixel(12, 20));
  TEST_ASSERT_TRUE(fb.getPixel(13, 20));
  TEST_ASSERT_TRUE(fb.getPixel(11, 24));
  TEST_ASSERT_TRUE(fb.getPixel(12, 24));
  TEST_ASSERT_TRUE(fb.getPixel(13, 24));
  // Left and right edge pixels (excluding the rounded corners).
  TEST_ASSERT_TRUE(fb.getPixel(10, 21));
  TEST_ASSERT_TRUE(fb.getPixel(10, 22));
  TEST_ASSERT_TRUE(fb.getPixel(10, 23));
  TEST_ASSERT_TRUE(fb.getPixel(14, 21));
  TEST_ASSERT_TRUE(fb.getPixel(14, 22));
  TEST_ASSERT_TRUE(fb.getPixel(14, 23));
}

void test_plan_mark_corners_are_ink() {
  Frame fb;
  fb.clear(false);
  render::HostCanvas canvas(fb);
  drawPlanMark(canvas, 10, 20);
  // The four corners of the 5×5 bounding box stay ink — that's what makes
  // the marker read as a ring rather than a square.
  TEST_ASSERT_FALSE(fb.getPixel(10, 20));
  TEST_ASSERT_FALSE(fb.getPixel(14, 20));
  TEST_ASSERT_FALSE(fb.getPixel(10, 24));
  TEST_ASSERT_FALSE(fb.getPixel(14, 24));
}

void test_plan_mark_interior_stays_ink() {
  Frame fb;
  fb.clear(false);
  render::HostCanvas canvas(fb);
  drawPlanMark(canvas, 10, 20);
  TEST_ASSERT_FALSE(fb.getPixel(12, 22));
  for (int yy = 21; yy <= 23; ++yy) {
    for (int xx = 11; xx <= 13; ++xx) {
      TEST_ASSERT_FALSE(fb.getPixel(xx, yy));
    }
  }
}

void test_plan_mark_does_not_leak_outside_5x5() {
  Frame fb;
  fb.clear(false);
  render::HostCanvas canvas(fb);
  drawPlanMark(canvas, 10, 20);
  TEST_ASSERT_FALSE(fb.getPixel(9, 22));
  TEST_ASSERT_FALSE(fb.getPixel(15, 22));
  TEST_ASSERT_FALSE(fb.getPixel(12, 19));
  TEST_ASSERT_FALSE(fb.getPixel(12, 25));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_plan_mark_rounded_edges_are_paper);
  RUN_TEST(test_plan_mark_corners_are_ink);
  RUN_TEST(test_plan_mark_interior_stays_ink);
  RUN_TEST(test_plan_mark_does_not_leak_outside_5x5);
  return UNITY_END();
}
