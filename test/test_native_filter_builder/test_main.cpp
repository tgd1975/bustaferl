// Structural check on buildStreamFilters / buildScheduleFilters /
// buildOebbFilter: every bus stream slot is populated (no zero-init holes),
// every identifier (RBL / DIVA) is non-zero, the S-Bahn stream is intentionally
// left default in the OGD/EFA tables (it is fetched via the ÖBB POST path), and
// the ÖBB filter itself is fully wired. A topology change that forgets to wire
// a stream shows up here, not at runtime as a silent "filter never matches".

#include "logic/filter_builder.h"

#include <unity.h>

using namespace bustaferl;

// The OGD bus streams occupy indices [0, STREAM_SBAHN_HBF).

void test_stream_filters_bus_streams_populated() {
  StreamFilter f[STREAM_COUNT];
  buildStreamFilters(f);
  for (int i = 0; i < STREAM_SBAHN_HBF; ++i) {
    TEST_ASSERT_NOT_EQUAL_MESSAGE(0, f[i].rbl, "bus filter rbl is zero");
    TEST_ASSERT_FALSE_MESSAGE(f[i].line.empty(), "bus filter line is empty");
    TEST_ASSERT_FALSE_MESSAGE(f[i].towards_prefix.empty(),
                              "bus filter towards_prefix is empty");
  }
}

void test_sbahn_stream_left_default_in_ogd_table() {
  StreamFilter f[STREAM_COUNT];
  buildStreamFilters(f);
  TEST_ASSERT_EQUAL_INT_MESSAGE(0, f[STREAM_SBAHN_HBF].rbl,
                                "S-Bahn must not carry an OGD RBL");
}

void test_schedule_filters_bus_streams_populated() {
  ScheduleStreamFilter f[STREAM_COUNT];
  buildScheduleFilters(f);
  for (int i = 0; i < STREAM_SBAHN_HBF; ++i) {
    TEST_ASSERT_NOT_EQUAL_MESSAGE(0, f[i].diva, "schedule filter diva is zero");
    TEST_ASSERT_FALSE_MESSAGE(f[i].line.empty(),
                              "schedule filter line is empty");
    TEST_ASSERT_FALSE_MESSAGE(f[i].direction_prefix.empty(),
                              "schedule filter direction_prefix is empty");
  }
}

void test_sbahn_stream_left_default_in_schedule_table() {
  ScheduleStreamFilter f[STREAM_COUNT];
  buildScheduleFilters(f);
  TEST_ASSERT_EQUAL_INT_MESSAGE(0, f[STREAM_SBAHN_HBF].diva,
                                "S-Bahn must have no EFA schedule DIVA");
}

void test_stream_filters_bus_rbls_are_distinct() {
  // Distinct RBLs — if two bus streams share one, the data map collapses two
  // streams onto one source and we lose half the display.
  StreamFilter f[STREAM_COUNT];
  buildStreamFilters(f);
  for (int i = 0; i < STREAM_SBAHN_HBF; ++i) {
    for (int j = i + 1; j < STREAM_SBAHN_HBF; ++j) {
      TEST_ASSERT_NOT_EQUAL_MESSAGE(f[i].rbl, f[j].rbl,
                                    "two bus streams share an RBL");
    }
  }
}

void test_oebb_filter_populated() {
  OebbStreamFilter o = buildOebbFilter();
  TEST_ASSERT_FALSE_MESSAGE(o.stb_eva.empty(), "oebb stb_eva empty");
  TEST_ASSERT_FALSE_MESSAGE(o.dir_eva.empty(), "oebb dir_eva empty");
  TEST_ASSERT_FALSE_MESSAGE(o.products.empty(), "oebb products empty");
  TEST_ASSERT_TRUE_MESSAGE(o.stb_eva != o.dir_eva,
                           "board and direction EVA must differ");
  TEST_ASSERT_TRUE(o.max_jny > 0);
}

void setUp() {}
void tearDown() {}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_stream_filters_bus_streams_populated);
  RUN_TEST(test_sbahn_stream_left_default_in_ogd_table);
  RUN_TEST(test_schedule_filters_bus_streams_populated);
  RUN_TEST(test_sbahn_stream_left_default_in_schedule_table);
  RUN_TEST(test_stream_filters_bus_rbls_are_distinct);
  RUN_TEST(test_oebb_filter_populated);
  return UNITY_END();
}
