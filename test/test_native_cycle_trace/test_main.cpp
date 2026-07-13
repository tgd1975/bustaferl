// Tier 1 — ring-buffer semantics for the diagnostic event memory
// (logic/cycle_trace). Newest-first ordering + wrap-around are what the
// diagnostic screens rely on; the higher-level stamping lives in cycle_runner.

#include "logic/cycle_trace.h"

#include <unity.h>

using namespace bustaferl;

namespace {

CycleRecord cyc(std::uint32_t at) {
  CycleRecord r;
  r.at = at;
  return r;
}

ErrorRecord err(std::uint32_t at, TraceError code) {
  ErrorRecord r;
  r.at = at;
  r.code = static_cast<std::uint8_t>(code);
  return r;
}

} // namespace

void setUp() {}
void tearDown() {}

void test_empty_trace_returns_null() {
  CycleTrace t;
  TEST_ASSERT_NULL(traceCycleAt(t, 0));
  TEST_ASSERT_NULL(traceErrorAt(t, 0));
}

void test_single_push_is_newest() {
  CycleTrace t;
  tracePushCycle(t, cyc(100));
  TEST_ASSERT_EQUAL(1, t.cycle_count);
  TEST_ASSERT_NOT_NULL(traceCycleAt(t, 0));
  TEST_ASSERT_EQUAL_UINT(100, traceCycleAt(t, 0)->at);
  TEST_ASSERT_NULL(traceCycleAt(t, 1));
}

void test_newest_first_ordering() {
  CycleTrace t;
  tracePushCycle(t, cyc(1));
  tracePushCycle(t, cyc(2));
  tracePushCycle(t, cyc(3));
  TEST_ASSERT_EQUAL_UINT(3, traceCycleAt(t, 0)->at);
  TEST_ASSERT_EQUAL_UINT(2, traceCycleAt(t, 1)->at);
  TEST_ASSERT_EQUAL_UINT(1, traceCycleAt(t, 2)->at);
  TEST_ASSERT_NULL(traceCycleAt(t, 3));
}

void test_wraps_and_drops_oldest() {
  CycleTrace t;
  // Push one more than capacity; entry 0 must have fallen off.
  for (int i = 0; i < CYCLE_TRACE_CAP + 1; ++i) {
    tracePushCycle(t, cyc(static_cast<std::uint32_t>(i)));
  }
  TEST_ASSERT_EQUAL(CYCLE_TRACE_CAP, t.cycle_count);
  // Newest is the last pushed (CAP), oldest survivor is 1 (0 dropped).
  TEST_ASSERT_EQUAL_UINT(CYCLE_TRACE_CAP, traceCycleAt(t, 0)->at);
  TEST_ASSERT_EQUAL_UINT(1, traceCycleAt(t, CYCLE_TRACE_CAP - 1)->at);
  TEST_ASSERT_NULL(traceCycleAt(t, CYCLE_TRACE_CAP));
}

void test_error_ring_independent() {
  CycleTrace t;
  tracePushError(t, err(10, TraceError::OebbAuth));
  tracePushError(t, err(20, TraceError::NtpFail));
  TEST_ASSERT_EQUAL(2, t.error_count);
  TEST_ASSERT_EQUAL_UINT(20, traceErrorAt(t, 0)->at);
  TEST_ASSERT_EQUAL(static_cast<int>(TraceError::NtpFail),
                    traceErrorAt(t, 0)->code);
  TEST_ASSERT_EQUAL_UINT(10, traceErrorAt(t, 1)->at);
  // Cycle ring untouched by error pushes.
  TEST_ASSERT_EQUAL(0, t.cycle_count);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_empty_trace_returns_null);
  RUN_TEST(test_single_push_is_newest);
  RUN_TEST(test_newest_first_ordering);
  RUN_TEST(test_wraps_and_drops_oldest);
  RUN_TEST(test_error_ring_independent);
  return UNITY_END();
}
