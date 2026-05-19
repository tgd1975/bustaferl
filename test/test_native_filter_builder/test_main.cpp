// Structural check on buildStreamFilters / buildScheduleFilters / buildOebbFilter.
//
// After v2 the OGD/EFA filter tables hold three populated bus-stream slots
// plus an intentionally-empty STREAM_SBAHN_HBF (rbl/diva == 0) — the S-Bahn
// stream lives on the HAFAS path and is built by buildOebbFilter. Tests
// enforce both halves of that contract so a topology change can't silently
// re-introduce an OGD/EFA call for the S-Bahn slot or zero out the OEBB
// filter.

#include "logic/filter_builder.h"

#include <unity.h>

using namespace bustaferl;

static constexpr int kOgdStreamCount = 3;

void test_stream_filters_three_ogd_slots_populated() {
  StreamFilter f[STREAM_COUNT];
  buildStreamFilters(f);
  for (int i = 0; i < kOgdStreamCount; ++i) {
    TEST_ASSERT_NOT_EQUAL_MESSAGE(0, f[i].rbl,
                                  "OGD stream filter rbl is zero");
    TEST_ASSERT_FALSE_MESSAGE(f[i].line.empty(),
                              "OGD stream filter line is empty");
    TEST_ASSERT_FALSE_MESSAGE(f[i].towards_prefix.empty(),
                              "OGD stream filter towards_prefix is empty");
  }
}

void test_stream_filters_sbahn_slot_left_default() {
  // STREAM_SBAHN_HBF is filled by snapshot_fetcher::fetchOebbStream
  // (HAFAS-Pfad). The OGD filter slot must stay zero so the OGD parser's
  // RBL match never accidentally writes the S-Bahn slot.
  StreamFilter f[STREAM_COUNT];
  buildStreamFilters(f);
  TEST_ASSERT_EQUAL_MESSAGE(0, f[STREAM_SBAHN_HBF].rbl,
                            "S-Bahn slot has non-zero OGD rbl");
  TEST_ASSERT_TRUE_MESSAGE(f[STREAM_SBAHN_HBF].line.empty(),
                           "S-Bahn slot has OGD line set");
}

void test_schedule_filters_three_ogd_slots_populated() {
  ScheduleStreamFilter f[STREAM_COUNT];
  buildScheduleFilters(f);
  for (int i = 0; i < kOgdStreamCount; ++i) {
    TEST_ASSERT_NOT_EQUAL_MESSAGE(0, f[i].diva,
                                  "schedule filter diva is zero");
    TEST_ASSERT_FALSE_MESSAGE(f[i].line.empty(),
                              "schedule filter line is empty");
    TEST_ASSERT_FALSE_MESSAGE(f[i].direction_prefix.empty(),
                              "schedule filter direction_prefix is empty");
  }
}

void test_schedule_filters_sbahn_slot_left_default() {
  // diva=0 → schedule_fetcher's distinct-diva loop skips the S-Bahn slot
  // (no EFA hint path, Variante 1 in §3.3 of the v2 plan).
  ScheduleStreamFilter f[STREAM_COUNT];
  buildScheduleFilters(f);
  TEST_ASSERT_EQUAL_MESSAGE(0, f[STREAM_SBAHN_HBF].diva,
                            "S-Bahn slot has non-zero schedule diva");
}

void test_stream_filters_ogd_rbls_are_distinct() {
  // Three distinct RBLs across the bus slots; sharing one would collapse
  // two streams onto a single OGD source.
  StreamFilter f[STREAM_COUNT];
  buildStreamFilters(f);
  for (int i = 0; i < kOgdStreamCount; ++i) {
    for (int j = i + 1; j < kOgdStreamCount; ++j) {
      TEST_ASSERT_NOT_EQUAL_MESSAGE(f[i].rbl, f[j].rbl,
                                    "two OGD streams share an RBL");
    }
  }
}

void test_buildOebbFilter_carries_config_constants() {
  OebbStreamFilter f = buildOebbFilter();
  TEST_ASSERT_EQUAL_STRING(OEBB_STBLOC_EXTID, f.stbloc_extid.c_str());
  TEST_ASSERT_EQUAL_STRING(OEBB_DIRLOC_EXTID, f.dirloc_extid.c_str());
  TEST_ASSERT_EQUAL_STRING(OEBB_JNYFLTR_PRODUCTS, f.products.c_str());
  TEST_ASSERT_EQUAL(OEBB_MAX_JNY, f.max_jny);
}

void setUp() {}
void tearDown() {}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_stream_filters_three_ogd_slots_populated);
  RUN_TEST(test_stream_filters_sbahn_slot_left_default);
  RUN_TEST(test_schedule_filters_three_ogd_slots_populated);
  RUN_TEST(test_schedule_filters_sbahn_slot_left_default);
  RUN_TEST(test_stream_filters_ogd_rbls_are_distinct);
  RUN_TEST(test_buildOebbFilter_carries_config_constants);
  return UNITY_END();
}
