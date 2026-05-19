// Network plan geometry — pixel-stamp asserts on marker centres + the
// vertical link between the two Atzg diamonds.

#include "render/network_plan.h"

#include <unity.h>

using namespace bustaferl;

void setUp() {}
void tearDown() {}

void test_drawNetworkPlanGeometry_returns_five_centres_evenly_spaced() {
  Frame fb;
  fb.clear(false);
  int centres[NETPLAN_COL_COUNT];
  drawNetworkPlanGeometry(fb, 18, 232, 364, centres);
  // Centres must be strictly increasing and evenly spaced. Width 364 /
  // 5 columns → step ~72 px between centres.
  for (int i = 1; i < NETPLAN_COL_COUNT; ++i) {
    TEST_ASSERT_GREATER_THAN(centres[i - 1], centres[i]);
  }
  // Spacing tolerance: ±2 px between adjacent centres.
  int step = centres[1] - centres[0];
  for (int i = 2; i < NETPLAN_COL_COUNT; ++i) {
    int s = centres[i] - centres[i - 1];
    TEST_ASSERT_INT_WITHIN(2, step, s);
  }
}

void test_drawNetworkPlanGeometry_marks_atzg_diamond_centres() {
  Frame fb;
  fb.clear(false);
  int centres[NETPLAN_COL_COUNT];
  drawNetworkPlanGeometry(fb, 18, 232, 364, centres);
  // Top row sits at y+5 = 237; bottom row at y+NETPLAN_HEIGHT-5 = 263.
  const int top_y = 232 + 5;
  const int bottom_y = 232 + NETPLAN_HEIGHT - 5;
  // The Atzg diamond at column 1: tip of diamond is the centre pixel.
  TEST_ASSERT_TRUE(fb.getPixel(centres[1], top_y));
  TEST_ASSERT_TRUE(fb.getPixel(centres[1], bottom_y));
}

void test_drawNetworkPlanGeometry_paints_vertical_link_between_atzg_diamonds() {
  Frame fb;
  fb.clear(false);
  int centres[NETPLAN_COL_COUNT];
  drawNetworkPlanGeometry(fb, 18, 232, 364, centres);
  const int top_y = 232 + 5;
  const int bottom_y = 232 + NETPLAN_HEIGHT - 5;
  // Sample the midpoint between the two diamonds on the Atzg column.
  int mid_y = (top_y + bottom_y) / 2;
  TEST_ASSERT_TRUE(fb.getPixel(centres[1], mid_y));
}

void test_drawNetworkPlanGeometry_paints_tull_big_square_centre() {
  Frame fb;
  fb.clear(false);
  int centres[NETPLAN_COL_COUNT];
  drawNetworkPlanGeometry(fb, 18, 232, 364, centres);
  const int bottom_y = 232 + NETPLAN_HEIGHT - 5;
  // Tull at column 3: 8×8 big square — centre pixel must be paper.
  TEST_ASSERT_TRUE(fb.getPixel(centres[3], bottom_y));
}

void test_drawNetworkPlanGeometry_paints_triangle_over_tull() {
  Frame fb;
  fb.clear(false);
  int centres[NETPLAN_COL_COUNT];
  drawNetworkPlanGeometry(fb, 18, 232, 364, centres);
  // Triangle base sits 9 px above bottom row; centre of triangle is at
  // centres[3], y = bottom_y - 9 .. bottom_y - 5. Top row of triangle is
  // all 5 paper pixels.
  const int bottom_y = 232 + NETPLAN_HEIGHT - 5;
  TEST_ASSERT_TRUE(fb.getPixel(centres[3] - 2, bottom_y - 9));
  TEST_ASSERT_TRUE(fb.getPixel(centres[3] - 1, bottom_y - 9));
  TEST_ASSERT_TRUE(fb.getPixel(centres[3], bottom_y - 9));
  TEST_ASSERT_TRUE(fb.getPixel(centres[3] + 1, bottom_y - 9));
  TEST_ASSERT_TRUE(fb.getPixel(centres[3] + 2, bottom_y - 9));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_drawNetworkPlanGeometry_returns_five_centres_evenly_spaced);
  RUN_TEST(test_drawNetworkPlanGeometry_marks_atzg_diamond_centres);
  RUN_TEST(
      test_drawNetworkPlanGeometry_paints_vertical_link_between_atzg_diamonds);
  RUN_TEST(test_drawNetworkPlanGeometry_paints_tull_big_square_centre);
  RUN_TEST(test_drawNetworkPlanGeometry_paints_triangle_over_tull);
  return UNITY_END();
}
