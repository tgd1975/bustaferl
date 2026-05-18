#include "logic/refresh_planner.h"

#include <cstring>
#include <unity.h>
#include <vector>

using namespace bustaferl;

static RefreshConfig cfg{400, 300, 7200, 80};

static std::vector<uint8_t> blankFrame() {
  return std::vector<uint8_t>(cfg.width * cfg.height / 8, 0xFF);
}

void test_identical_frames_no_refresh() {
  auto a = blankFrame();
  auto b = blankFrame();
  auto d = planRefresh(a.data(), b.data(), true, 1000, 999, 0, cfg);
  TEST_ASSERT_EQUAL(static_cast<int>(RefreshKind::None),
                    static_cast<int>(d.kind));
}

void test_prev_invalid_forces_light_full() {
  auto a = blankFrame();
  auto b = blankFrame();
  auto d = planRefresh(a.data(), b.data(), false, 1000, 999, 0, cfg);
  TEST_ASSERT_EQUAL(static_cast<int>(RefreshKind::LightFull),
                    static_cast<int>(d.kind));
}

void test_small_change_yields_partial_with_8px_bbox() {
  auto a = blankFrame();
  auto b = blankFrame();
  // flip a single bit at (x=17, y=42) → bbox should align to x=16, w=8.
  const int stride = cfg.width / 8;
  b[42 * stride + (17 / 8)] ^= (0x80 >> (17 & 7));
  auto d = planRefresh(a.data(), b.data(), true, 1000, 999, 0, cfg);
  TEST_ASSERT_EQUAL(static_cast<int>(RefreshKind::Partial),
                    static_cast<int>(d.kind));
  TEST_ASSERT_EQUAL(16, d.bbox.x);
  TEST_ASSERT_EQUAL(42, d.bbox.y);
  TEST_ASSERT_EQUAL(8, d.bbox.w);
  TEST_ASSERT_EQUAL(1, d.bbox.h);
}

void test_old_light_full_triggers_light_full() {
  auto a = blankFrame();
  auto b = blankFrame();
  b[100] ^= 0x01;
  auto d = planRefresh(a.data(), b.data(), true,
                       /*now=*/100000,
                       /*last_lf=*/100000 - 7300, 0, cfg);
  TEST_ASSERT_EQUAL(static_cast<int>(RefreshKind::LightFull),
                    static_cast<int>(d.kind));
}

void test_partial_hardcap_triggers_light_full() {
  auto a = blankFrame();
  auto b = blankFrame();
  b[100] ^= 0x01;
  auto d = planRefresh(a.data(), b.data(), true, 1000, 999, 80, cfg);
  TEST_ASSERT_EQUAL(static_cast<int>(RefreshKind::LightFull),
                    static_cast<int>(d.kind));
}

void test_bbox_spans_multiple_bytes() {
  auto a = blankFrame();
  auto b = blankFrame();
  const int stride = cfg.width / 8;
  // change pixels at (5,10) and (200,10)
  b[10 * stride + 0] ^= 0x04;
  b[10 * stride + (200 / 8)] ^= 0x80;
  auto d = planRefresh(a.data(), b.data(), true, 1000, 999, 0, cfg);
  TEST_ASSERT_EQUAL(static_cast<int>(RefreshKind::Partial),
                    static_cast<int>(d.kind));
  TEST_ASSERT_EQUAL(0, d.bbox.x);
  TEST_ASSERT_EQUAL(10, d.bbox.y);
  TEST_ASSERT_TRUE(d.bbox.w >= 208);
  TEST_ASSERT_EQUAL(0, d.bbox.w & 7);
}

void setUp() {}
void tearDown() {}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_identical_frames_no_refresh);
  RUN_TEST(test_prev_invalid_forces_light_full);
  RUN_TEST(test_small_change_yields_partial_with_8px_bbox);
  RUN_TEST(test_old_light_full_triggers_light_full);
  RUN_TEST(test_partial_hardcap_triggers_light_full);
  RUN_TEST(test_bbox_spans_multiple_bytes);
  return UNITY_END();
}
