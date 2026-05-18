// Structural check on buildStreamFilters / buildScheduleFilters: every stream
// slot is populated (no zero-init holes), every identifier (RBL / DIVA) is
// non-zero, and every line / towards string is non-empty. A topology change
// that forgets to wire one of the five streams shows up here, not at runtime
// as a silent "filter never matches".

#include "logic/filter_builder.h"

#include <unity.h>

using namespace bustaferl;

void test_stream_filters_all_streams_populated() {
  StreamFilter f[STREAM_COUNT];
  buildStreamFilters(f);
  for (int i = 0; i < STREAM_COUNT; ++i) {
    TEST_ASSERT_NOT_EQUAL_MESSAGE(0, f[i].rbl, "stream filter rbl is zero");
    TEST_ASSERT_FALSE_MESSAGE(f[i].line.empty(), "stream filter line is empty");
    TEST_ASSERT_FALSE_MESSAGE(f[i].towards_prefix.empty(),
                              "stream filter towards_prefix is empty");
  }
}

void test_schedule_filters_all_streams_populated() {
  ScheduleStreamFilter f[STREAM_COUNT];
  buildScheduleFilters(f);
  for (int i = 0; i < STREAM_COUNT; ++i) {
    TEST_ASSERT_NOT_EQUAL_MESSAGE(0, f[i].diva, "schedule filter diva is zero");
    TEST_ASSERT_FALSE_MESSAGE(f[i].line.empty(),
                              "schedule filter line is empty");
    TEST_ASSERT_FALSE_MESSAGE(f[i].direction_prefix.empty(),
                              "schedule filter direction_prefix is empty");
  }
}

void test_stream_filters_rbls_are_distinct() {
  // Five distinct RBLs — if two stream slots end up sharing one, the data map
  // collapses two streams onto one source and we lose half the display.
  StreamFilter f[STREAM_COUNT];
  buildStreamFilters(f);
  for (int i = 0; i < STREAM_COUNT; ++i) {
    for (int j = i + 1; j < STREAM_COUNT; ++j) {
      TEST_ASSERT_NOT_EQUAL_MESSAGE(f[i].rbl, f[j].rbl,
                                    "two streams share an RBL");
    }
  }
}

void setUp() {}
void tearDown() {}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_stream_filters_all_streams_populated);
  RUN_TEST(test_schedule_filters_all_streams_populated);
  RUN_TEST(test_stream_filters_rbls_are_distinct);
  return UNITY_END();
}
