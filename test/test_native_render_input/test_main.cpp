// composeRenderInput(snap, schedule, overlay, now) — exercises the three
// overlay paths the cycle hits in practice: None / Stale / FilterDead.

#include "logic/render_input.h"
#include "logic/slot_merger.h"

#include <unity.h>

using namespace bustaferl;

namespace {

constexpr time_t kNow = 1700000000;

ScheduleSnapshot makeFreshSchedule(time_t t0, time_t t1) {
  ScheduleSnapshot s;
  s.fetched_at = kNow - 3600;
  s.hint[STREAM_58A_ATZ].first_tomorrow[0] = t0;
  s.hint[STREAM_58A_ATZ].first_tomorrow[1] = t1;
  return s;
}

Departure makeRealtime(time_t when) {
  Departure d;
  d.when = when;
  d.source = DepartureSource::Realtime;
  d.valid = true;
  return d;
}

} // namespace

void setUp() {}
void tearDown() {}

void test_overlay_none_runs_merge() {
  // Realtime empty, hints available → merge fills slot from hint.
  StreamSnapshot snap;
  snap.api_ok = true;
  ScheduleSnapshot sched = makeFreshSchedule(kNow + 4000, kNow + 5000);

  RenderInput in = composeRenderInput(snap, sched, OverlayKind::None, kNow);

  TEST_ASSERT_EQUAL(OverlayKind::None, in.overlay);
  TEST_ASSERT_TRUE(in.snapshot.stream[STREAM_58A_ATZ].slot[0].valid);
  TEST_ASSERT_EQUAL_INT64(kNow + 4000,
                          in.snapshot.stream[STREAM_58A_ATZ].slot[0].when);
  TEST_ASSERT_EQUAL(DepartureSource::Hint,
                    in.snapshot.stream[STREAM_58A_ATZ].slot[0].source);
}

void test_overlay_stale_skips_merge() {
  // Realtime empty, hints available — but Stale overlay must NOT let hints
  // leak through. The snapshot is passed through untouched.
  StreamSnapshot snap;
  ScheduleSnapshot sched = makeFreshSchedule(kNow + 4000, kNow + 5000);

  RenderInput in = composeRenderInput(snap, sched, OverlayKind::Stale, kNow);

  TEST_ASSERT_EQUAL(OverlayKind::Stale, in.overlay);
  TEST_ASSERT_FALSE(in.snapshot.stream[STREAM_58A_ATZ].slot[0].valid);
  TEST_ASSERT_FALSE(in.snapshot.stream[STREAM_58A_ATZ].slot[1].valid);
}

void test_overlay_filter_dead_runs_merge() {
  // FilterDead overlay still lets the merger run — the renderer paints the
  // overlay on top of whatever slots we hand it.
  StreamSnapshot snap;
  snap.stream[STREAM_58A_ATZ].slot[0] = makeRealtime(kNow + 600);
  ScheduleSnapshot sched = makeFreshSchedule(kNow + 4000, kNow + 5000);

  RenderInput in =
      composeRenderInput(snap, sched, OverlayKind::FilterDead, kNow);

  TEST_ASSERT_EQUAL(OverlayKind::FilterDead, in.overlay);
  TEST_ASSERT_TRUE(in.snapshot.stream[STREAM_58A_ATZ].slot[0].valid);
  TEST_ASSERT_EQUAL_INT64(kNow + 600,
                          in.snapshot.stream[STREAM_58A_ATZ].slot[0].when);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_overlay_none_runs_merge);
  RUN_TEST(test_overlay_stale_skips_merge);
  RUN_TEST(test_overlay_filter_dead_runs_merge);
  return UNITY_END();
}
