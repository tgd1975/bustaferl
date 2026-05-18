#include "data/ScheduleHint.h"
#include "data/StreamSnapshot.h"
#include "logic/slot_merger.h"

#include <unity.h>

using namespace bustaferl;

namespace {

Departure makeDep(time_t when, bool is_realtime) {
  Departure d;
  d.when = when;
  d.is_realtime = is_realtime;
  d.valid = true;
  return d;
}

constexpr time_t kNow = 1700000000;

ScheduleSnapshot makeFreshSchedule(time_t t0, time_t t1) {
  ScheduleSnapshot s;
  s.fetched_at = kNow - 3600;
  s.hint[STREAM_58A_ATZ].first_tomorrow[0] = t0;
  s.hint[STREAM_58A_ATZ].first_tomorrow[1] = t1;
  return s;
}

} // namespace

void setUp() {}
void tearDown() {}

void test_realtime_full_passes_through_unchanged() {
  StreamSnapshot snap;
  snap.api_ok = true;
  snap.stream[STREAM_58A_ATZ].slot[0] = makeDep(kNow + 600, true);
  snap.stream[STREAM_58A_ATZ].slot[1] = makeDep(kNow + 1200, true);
  ScheduleSnapshot sched = makeFreshSchedule(kNow + 4000, kNow + 5000);

  StreamSnapshot out = mergeSlots(snap, sched, kNow);
  TEST_ASSERT_EQUAL_INT64(kNow + 600, out.stream[STREAM_58A_ATZ].slot[0].when);
  TEST_ASSERT_EQUAL_INT64(kNow + 1200, out.stream[STREAM_58A_ATZ].slot[1].when);
  TEST_ASSERT_TRUE(out.stream[STREAM_58A_ATZ].slot[0].is_realtime);
}

void test_realtime_empty_falls_back_to_hints() {
  StreamSnapshot snap;
  ScheduleSnapshot sched = makeFreshSchedule(kNow + 4000, kNow + 5000);

  StreamSnapshot out = mergeSlots(snap, sched, kNow);
  TEST_ASSERT_TRUE(out.stream[STREAM_58A_ATZ].slot[0].valid);
  TEST_ASSERT_EQUAL_INT64(kNow + 4000, out.stream[STREAM_58A_ATZ].slot[0].when);
  TEST_ASSERT_EQUAL_INT64(kNow + 5000, out.stream[STREAM_58A_ATZ].slot[1].when);
  TEST_ASSERT_FALSE(out.stream[STREAM_58A_ATZ].slot[0].is_realtime);
}

void test_one_realtime_plus_hint_merges_chronologically() {
  // The "schrittweise" case from CONCEPT.md §12.4: realtime captures the
  // first morning departure, hint still supplies the next one.
  StreamSnapshot snap;
  snap.stream[STREAM_58A_ATZ].slot[0] = makeDep(kNow + 4000, true);
  ScheduleSnapshot sched = makeFreshSchedule(kNow + 4000, kNow + 5000);

  StreamSnapshot out = mergeSlots(snap, sched, kNow);
  TEST_ASSERT_EQUAL_INT64(kNow + 4000, out.stream[STREAM_58A_ATZ].slot[0].when);
  // dedup: realtime wins over the identical hint, second slot must be the
  // *next* hint value (5000), not a stale duplicate.
  TEST_ASSERT_EQUAL_INT64(kNow + 5000, out.stream[STREAM_58A_ATZ].slot[1].when);
  TEST_ASSERT_TRUE(out.stream[STREAM_58A_ATZ].slot[0].is_realtime);
  TEST_ASSERT_FALSE(out.stream[STREAM_58A_ATZ].slot[1].is_realtime);
}

void test_past_realtime_and_past_hint_are_dropped() {
  StreamSnapshot snap;
  snap.stream[STREAM_58A_ATZ].slot[0] = makeDep(kNow - 60, true); // past
  snap.stream[STREAM_58A_ATZ].slot[1] = makeDep(kNow + 600, true);
  ScheduleSnapshot sched = makeFreshSchedule(kNow - 300, kNow + 5000);

  StreamSnapshot out = mergeSlots(snap, sched, kNow);
  TEST_ASSERT_EQUAL_INT64(kNow + 600, out.stream[STREAM_58A_ATZ].slot[0].when);
  TEST_ASSERT_EQUAL_INT64(kNow + 5000, out.stream[STREAM_58A_ATZ].slot[1].when);
}

void test_stale_schedule_is_ignored() {
  StreamSnapshot snap; // empty
  ScheduleSnapshot sched = makeFreshSchedule(kNow + 4000, kNow + 5000);
  sched.fetched_at = kNow - (SCHEDULE_HINT_MAX_AGE_S + 1);

  StreamSnapshot out = mergeSlots(snap, sched, kNow);
  TEST_ASSERT_FALSE(out.stream[STREAM_58A_ATZ].slot[0].valid);
  TEST_ASSERT_FALSE(out.stream[STREAM_58A_ATZ].slot[1].valid);
}

void test_never_fetched_schedule_is_ignored() {
  StreamSnapshot snap;
  ScheduleSnapshot sched; // fetched_at = 0
  sched.hint[STREAM_58A_ATZ].first_tomorrow[0] = kNow + 4000;

  StreamSnapshot out = mergeSlots(snap, sched, kNow);
  TEST_ASSERT_FALSE(out.stream[STREAM_58A_ATZ].slot[0].valid);
}

void test_realtime_earlier_than_hint_still_orders_correctly() {
  // Edge: realtime departure arrives before the first hint value. Output must
  // be [realtime, hint0] in chronological order regardless of insertion order.
  StreamSnapshot snap;
  snap.stream[STREAM_58A_ATZ].slot[0] = makeDep(kNow + 1000, true);
  ScheduleSnapshot sched = makeFreshSchedule(kNow + 4000, kNow + 5000);

  StreamSnapshot out = mergeSlots(snap, sched, kNow);
  TEST_ASSERT_EQUAL_INT64(kNow + 1000, out.stream[STREAM_58A_ATZ].slot[0].when);
  TEST_ASSERT_EQUAL_INT64(kNow + 4000, out.stream[STREAM_58A_ATZ].slot[1].when);
}

void test_preserves_carry_fields() {
  // api_ok / rbl_responded / filter_matched must survive the merge unchanged
  // — downstream stale and filter-health logic still depends on them.
  StreamSnapshot snap;
  snap.api_ok = true;
  snap.stream[STREAM_58B_ATZ].rbl_responded = true;
  snap.stream[STREAM_58B_ATZ].filter_matched = false;
  ScheduleSnapshot sched;

  StreamSnapshot out = mergeSlots(snap, sched, kNow);
  TEST_ASSERT_TRUE(out.api_ok);
  TEST_ASSERT_TRUE(out.stream[STREAM_58B_ATZ].rbl_responded);
  TEST_ASSERT_FALSE(out.stream[STREAM_58B_ATZ].filter_matched);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_realtime_full_passes_through_unchanged);
  RUN_TEST(test_realtime_empty_falls_back_to_hints);
  RUN_TEST(test_one_realtime_plus_hint_merges_chronologically);
  RUN_TEST(test_past_realtime_and_past_hint_are_dropped);
  RUN_TEST(test_stale_schedule_is_ignored);
  RUN_TEST(test_never_fetched_schedule_is_ignored);
  RUN_TEST(test_realtime_earlier_than_hint_still_orders_correctly);
  RUN_TEST(test_preserves_carry_fields);
  return UNITY_END();
}
