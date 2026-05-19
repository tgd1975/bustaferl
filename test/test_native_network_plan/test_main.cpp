// Network plan geometry — pixel-stamp asserts on marker centres + the
// vertical link between the two Atzg diamonds. Exercises the production
// drawNetworkPlan() via HostCanvas.

#include "render/canvas_host.h"
#include "render/network_plan.h"

#include <unity.h>

using namespace bustaferl;

void setUp() {}
void tearDown() {}

void test_drawNetworkPlan_returns_five_centres_evenly_spaced() {
  Frame fb;
  fb.clear(false);
  render::HostCanvas canvas(fb);
  int centres[NETPLAN_COL_COUNT];
  drawNetworkPlan(canvas, 18, 232, 364, centres);
  for (int i = 1; i < NETPLAN_COL_COUNT; ++i) {
    TEST_ASSERT_GREATER_THAN(centres[i - 1], centres[i]);
  }
  int step = centres[1] - centres[0];
  for (int i = 2; i < NETPLAN_COL_COUNT; ++i) {
    int s = centres[i] - centres[i - 1];
    TEST_ASSERT_INT_WITHIN(2, step, s);
  }
}

void test_drawNetworkPlan_marks_atzg_diamond_centres() {
  Frame fb;
  fb.clear(false);
  render::HostCanvas canvas(fb);
  int centres[NETPLAN_COL_COUNT];
  drawNetworkPlan(canvas, 18, 232, 364, centres);
  const int top_y = 232 + 5;
  const int bottom_y = 232 + NETPLAN_HEIGHT - 5;
  TEST_ASSERT_TRUE(fb.getPixel(centres[1], top_y));
  TEST_ASSERT_TRUE(fb.getPixel(centres[1], bottom_y));
}

void test_drawNetworkPlan_paints_vertical_link_between_atzg_diamonds() {
  Frame fb;
  fb.clear(false);
  render::HostCanvas canvas(fb);
  int centres[NETPLAN_COL_COUNT];
  drawNetworkPlan(canvas, 18, 232, 364, centres);
  const int top_y = 232 + 5;
  const int bottom_y = 232 + NETPLAN_HEIGHT - 5;
  int mid_y = (top_y + bottom_y) / 2;
  TEST_ASSERT_TRUE(fb.getPixel(centres[1], mid_y));
}

void test_drawNetworkPlan_paints_tull_big_square_centre() {
  Frame fb;
  fb.clear(false);
  render::HostCanvas canvas(fb);
  int centres[NETPLAN_COL_COUNT];
  drawNetworkPlan(canvas, 18, 232, 364, centres);
  const int bottom_y = 232 + NETPLAN_HEIGHT - 5;
  TEST_ASSERT_TRUE(fb.getPixel(centres[3], bottom_y));
}

void test_drawNetworkPlan_paints_triangle_over_tull() {
  Frame fb;
  fb.clear(false);
  render::HostCanvas canvas(fb);
  int centres[NETPLAN_COL_COUNT];
  drawNetworkPlan(canvas, 18, 232, 364, centres);
  const int bottom_y = 232 + NETPLAN_HEIGHT - 5;
  // Top row of the 5×5 triangle sprite is all paper.
  TEST_ASSERT_TRUE(fb.getPixel(centres[3] - 2, bottom_y - 9));
  TEST_ASSERT_TRUE(fb.getPixel(centres[3] - 1, bottom_y - 9));
  TEST_ASSERT_TRUE(fb.getPixel(centres[3], bottom_y - 9));
  TEST_ASSERT_TRUE(fb.getPixel(centres[3] + 1, bottom_y - 9));
  TEST_ASSERT_TRUE(fb.getPixel(centres[3] + 2, bottom_y - 9));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_drawNetworkPlan_returns_five_centres_evenly_spaced);
  RUN_TEST(test_drawNetworkPlan_marks_atzg_diamond_centres);
  RUN_TEST(test_drawNetworkPlan_paints_vertical_link_between_atzg_diamonds);
  RUN_TEST(test_drawNetworkPlan_paints_tull_big_square_centre);
  RUN_TEST(test_drawNetworkPlan_paints_triangle_over_tull);
  return UNITY_END();
}
