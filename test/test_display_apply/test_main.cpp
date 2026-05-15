#include <cstdint>
#include <unity.h>
#include <vector>

#include "hal/IDisplay.h"
#include "hal/IPersistentStore.h"
#include "logic/display_apply.h"
#include "logic/refresh_planner.h"

using namespace bustaferl;

namespace {

class FakeDisplay : public IDisplay {
public:
  int full_calls = 0;
  int partial_calls = 0;
  int light_full_calls = 0;
  int deep_clean_calls = 0;
  Bbox last_partial_bbox{};

  void drawFull(const uint8_t *) override { ++full_calls; }
  void drawPartial(const uint8_t *, const Bbox &b) override {
    ++partial_calls;
    last_partial_bbox = b;
  }
  void lightFull(const uint8_t *) override { ++light_full_calls; }
  void deepClean(const uint8_t *) override { ++deep_clean_calls; }
};

std::vector<uint8_t> dummyFb() { return std::vector<uint8_t>(15000, 0xFF); }

} // namespace

void test_none_changes_nothing() {
  FakeDisplay d;
  PersistedMeta m;
  m.partial_count = 7;
  m.last_light_full = 100;
  auto fb = dummyFb();
  RefreshDecision rd;
  rd.kind = RefreshKind::None;
  applyDisplayDecision(d, rd, fb.data(), m, 1000);
  TEST_ASSERT_EQUAL_INT(0, d.partial_calls);
  TEST_ASSERT_EQUAL_INT(0, d.light_full_calls);
  TEST_ASSERT_EQUAL_INT(0, d.deep_clean_calls);
  TEST_ASSERT_EQUAL_UINT(7, m.partial_count);
  TEST_ASSERT_EQUAL_INT64(100, m.last_light_full);
}

void test_partial_increments_counter_and_forwards_bbox() {
  FakeDisplay d;
  PersistedMeta m;
  m.partial_count = 4;
  auto fb = dummyFb();
  RefreshDecision rd;
  rd.kind = RefreshKind::Partial;
  rd.bbox = Bbox{16, 42, 32, 8};
  applyDisplayDecision(d, rd, fb.data(), m, 1000);
  TEST_ASSERT_EQUAL_INT(1, d.partial_calls);
  TEST_ASSERT_EQUAL_UINT(5, m.partial_count);
  TEST_ASSERT_EQUAL_INT(16, d.last_partial_bbox.x);
  TEST_ASSERT_EQUAL_INT(42, d.last_partial_bbox.y);
  TEST_ASSERT_EQUAL_INT(32, d.last_partial_bbox.w);
  TEST_ASSERT_EQUAL_INT(8, d.last_partial_bbox.h);
}

void test_light_full_resets_counters_and_stamps_now() {
  FakeDisplay d;
  PersistedMeta m;
  m.partial_count = 80;
  m.last_light_full = 0;
  auto fb = dummyFb();
  RefreshDecision rd;
  rd.kind = RefreshKind::LightFull;
  applyDisplayDecision(d, rd, fb.data(), m, 1700000123);
  TEST_ASSERT_EQUAL_INT(1, d.light_full_calls);
  TEST_ASSERT_EQUAL_UINT(0, m.partial_count);
  TEST_ASSERT_EQUAL_INT64(1700000123, m.last_light_full);
}

void test_deep_clean_resets_all_and_stamps_both_timestamps() {
  FakeDisplay d;
  PersistedMeta m;
  m.partial_count = 12;
  m.last_light_full = 100;
  m.last_deep_clean = 200;
  auto fb = dummyFb();
  RefreshDecision rd;
  rd.kind = RefreshKind::DeepClean;
  applyDisplayDecision(d, rd, fb.data(), m, 1700000999);
  TEST_ASSERT_EQUAL_INT(1, d.deep_clean_calls);
  TEST_ASSERT_EQUAL_UINT(0, m.partial_count);
  TEST_ASSERT_EQUAL_INT64(1700000999, m.last_light_full);
  TEST_ASSERT_EQUAL_INT64(1700000999, m.last_deep_clean);
}

void setUp() {}
void tearDown() {}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_none_changes_nothing);
  RUN_TEST(test_partial_increments_counter_and_forwards_bbox);
  RUN_TEST(test_light_full_resets_counters_and_stamps_now);
  RUN_TEST(test_deep_clean_resets_all_and_stamps_both_timestamps);
  return UNITY_END();
}
