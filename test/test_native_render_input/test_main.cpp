// composeRenderInput(snap, schedule, overlay, now) — exercises the three
// overlay paths the cycle hits in practice: None / Stale.

#include "logic/render_input.h"
#include "logic/slot_merger.h"

#include <cstring>
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

void test_sbahn_line_label_survives_merge_and_banner_flags_default_off() {
  // The S-Bahn stream has no schedule hints, so a realtime departure with a
  // per-slot line label must pass through composeRenderInput's merge untouched.
  StreamSnapshot snap;
  snap.stream[STREAM_SBAHN_HBF].slot[0] = makeRealtime(kNow + 600);
  std::strncpy(snap.stream[STREAM_SBAHN_HBF].slot[0].line_label, "S2", 6);
  ScheduleSnapshot sched; // empty — no hints for any stream

  RenderInput in = composeRenderInput(snap, sched, OverlayKind::None, kNow);

  TEST_ASSERT_EQUAL(OverlayKind::None, in.overlay);
  TEST_ASSERT_TRUE(in.snapshot.stream[STREAM_SBAHN_HBF].slot[0].valid);
  TEST_ASSERT_EQUAL_STRING(
      "S2", in.snapshot.stream[STREAM_SBAHN_HBF].slot[0].line_label);
  // Per-section banner flags default off and are set by the caller, not compose.
  TEST_ASSERT_FALSE(in.filter_dead_58b);
  TEST_ASSERT_FALSE(in.oebb_auth_dead);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_overlay_none_runs_merge);
  RUN_TEST(test_overlay_stale_skips_merge);
  RUN_TEST(test_sbahn_line_label_survives_merge_and_banner_flags_default_off);
  return UNITY_END();
}
