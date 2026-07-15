// selectDisplayState + composeRenderInput coverage. Only the error/placeholder
// screens are states (Auth / Boot / Offline); everything with data — fresh,
// scheduled-only, stale, or a far-future gap — is the Normal board. These tests
// pin that: the cases that used to become Stale / Quiet / Night must now all
// resolve to Normal so the board keeps showing the next departure's real time.

#include "config.h"
#include "logic/render_input.h"

#include <cstring>
#include <unity.h>

using namespace bustaferl;

namespace {

constexpr time_t kNow = 1700000000; // 2023-11-14 22:13:20 UTC

// Realtime departure helper.
Departure makeRealtime(time_t when) {
  Departure d;
  d.when = when;
  d.source = DepartureSource::Realtime;
  d.valid = true;
  return d;
}

SelectorSignals baseSignals() {
  SelectorSignals sig;
  sig.first_render_ever = false;
  sig.auth_error_seen = false;
  sig.wifi_up = true;
  sig.now = kNow;
  sig.last_success = kNow - 30; // fresh
  return sig;
}

PersistedMeta baseMeta() {
  PersistedMeta m;
  m.has_any_data = true;
  m.last_success_at = kNow - 30;
  return m;
}

} // namespace

void setUp() {
  // Pin TZ so HH:MM formatting is deterministic across CI hosts.
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();
}
void tearDown() {}

// ----- State-selector: one test per state -----

void test_state_auth_when_auth_error_seen() {
  StreamSnapshot snap;
  ScheduleSnapshot sched;
  PersistedMeta meta = baseMeta();
  SelectorSignals sig = baseSignals();
  sig.auth_error_seen = true;
  TEST_ASSERT_EQUAL(DisplayState::Auth,
                    selectDisplayState(snap, sched, meta, sig));
}

void test_state_boot_when_first_render_ever() {
  StreamSnapshot snap;
  ScheduleSnapshot sched;
  PersistedMeta meta;
  meta.has_any_data = false;
  SelectorSignals sig = baseSignals();
  sig.first_render_ever = true;
  sig.last_success = 0;
  TEST_ASSERT_EQUAL(DisplayState::Boot,
                    selectDisplayState(snap, sched, meta, sig));
}

void test_state_offline_when_wifi_down_and_stale() {
  StreamSnapshot snap;
  ScheduleSnapshot sched;
  PersistedMeta meta = baseMeta();
  meta.last_success_at = kNow - (OFFLINE_THRESHOLD_S + 10);
  SelectorSignals sig = baseSignals();
  sig.wifi_up = false;
  sig.last_success = kNow - (OFFLINE_THRESHOLD_S + 10);
  TEST_ASSERT_EQUAL(DisplayState::Offline,
                    selectDisplayState(snap, sched, meta, sig));
}

// Data old (wifi up) used to become Stale — now stays Normal. The board keeps
// the last departure it had (or the schedule-backed merge) rather than a blank
// "??:??" screen; the warm cycle's redraw guard decides whether to repaint.
void test_state_normal_when_long_since_success() {
  StreamSnapshot snap;
  ScheduleSnapshot sched;
  PersistedMeta meta = baseMeta();
  meta.last_success_at = kNow - (STALE_THRESHOLD_V2_S + 10);
  SelectorSignals sig = baseSignals();
  sig.wifi_up = true; // wifi up, but data is old
  sig.last_success = kNow - (STALE_THRESHOLD_V2_S + 10);
  TEST_ASSERT_EQUAL(DisplayState::Normal,
                    selectDisplayState(snap, sched, meta, sig));
}

// A departure hours out used to become Quiet ("KEINE ABFAHRTEN") — now stays
// Normal so the board shows that departure's real time. This is the exact case
// the removal was about: there is always a next departure, show it.
void test_state_normal_when_next_departure_far_out() {
  StreamSnapshot snap;
  snap.stream[STREAM_58A_ATZ].slot[0] = makeRealtime(kNow + 4 * 3600); // 4 h
  ScheduleSnapshot sched;
  PersistedMeta meta = baseMeta();
  SelectorSignals sig = baseSignals();
  TEST_ASSERT_EQUAL(DisplayState::Normal,
                    selectDisplayState(snap, sched, meta, sig));
}

// Overnight: no realtime, only tomorrow's scheduled first departures. Used to
// become Night/Quiet; now Normal so the board shows the scheduled times.
void test_state_normal_overnight_schedule_only() {
  time_t night_now = 1700016600; // 2023-11-14 02:30 Vienna local (night)
  StreamSnapshot empty;          // no realtime departures overnight
  ScheduleSnapshot sched;
  sched.fetched_at = night_now - 3600; // fresh, well within 48 h
  sched.hint[STREAM_58A_ATZ].first_tomorrow[0] = night_now + 600;
  sched.hint[STREAM_58A_ATZ].first_tomorrow[1] = night_now + 1200;

  PersistedMeta meta = baseMeta();
  meta.last_success_at = night_now - 60;
  SelectorSignals sig = baseSignals();
  sig.now = night_now;
  sig.last_success = night_now - 60;
  TEST_ASSERT_EQUAL(DisplayState::Normal,
                    selectDisplayState(empty, sched, meta, sig));
}

void test_state_normal_when_realtime_imminent() {
  StreamSnapshot snap;
  snap.stream[STREAM_58A_ATZ].slot[0] = makeRealtime(kNow + 300);
  snap.api_ok = true;
  ScheduleSnapshot sched;
  PersistedMeta meta = baseMeta();
  SelectorSignals sig = baseSignals();
  TEST_ASSERT_EQUAL(DisplayState::Normal,
                    selectDisplayState(snap, sched, meta, sig));
}

// Empty everything (no realtime, no schedule) still resolves to Normal — the
// board renders "--:--" placeholders, never a dedicated blank state.
void test_state_normal_when_no_data_at_all() {
  StreamSnapshot empty;
  ScheduleSnapshot sched;
  PersistedMeta meta = baseMeta();
  SelectorSignals sig = baseSignals();
  TEST_ASSERT_EQUAL(DisplayState::Normal,
                    selectDisplayState(empty, sched, meta, sig));
}

// Auth wins over Boot — important for cold-boot-with-bad-AID UX.
void test_state_auth_dominates_boot() {
  StreamSnapshot snap;
  ScheduleSnapshot sched;
  PersistedMeta meta;
  meta.has_any_data = false;
  SelectorSignals sig = baseSignals();
  sig.first_render_ever = true;
  sig.auth_error_seen = true;
  TEST_ASSERT_EQUAL(DisplayState::Auth,
                    selectDisplayState(snap, sched, meta, sig));
}

// ----- composeRenderInput -----

void test_compose_boot_carries_version_string() {
  StreamSnapshot snap;
  ScheduleSnapshot sched;
  PersistedMeta meta = baseMeta();
  RenderInput in =
      composeRenderInput(DisplayState::Boot, snap, sched, meta, kNow);
  TEST_ASSERT_EQUAL(DisplayState::Boot, in.state);
  TEST_ASSERT_NOT_NULL(in.firmware_version);
  TEST_ASSERT_NOT_NULL(std::strstr(in.firmware_version, "v2"));
}

void test_compose_offline_fills_retry_in_s() {
  StreamSnapshot snap;
  ScheduleSnapshot sched;
  PersistedMeta meta = baseMeta();
  meta.last_success_at = kNow - 60; // 60 s ago
  RenderInput in =
      composeRenderInput(DisplayState::Offline, snap, sched, meta, kNow);
  TEST_ASSERT_EQUAL(DisplayState::Offline, in.state);
  TEST_ASSERT_EQUAL_INT64(kNow - 60, in.last_fetch_at);
  TEST_ASSERT_EQUAL_INT(OFFLINE_THRESHOLD_S - 60, in.retry_in_s);
}

void test_compose_offline_clamps_retry_at_zero() {
  StreamSnapshot snap;
  ScheduleSnapshot sched;
  PersistedMeta meta = baseMeta();
  meta.last_success_at = kNow - 10000; // way past threshold
  RenderInput in =
      composeRenderInput(DisplayState::Offline, snap, sched, meta, kNow);
  TEST_ASSERT_EQUAL_INT(0, in.retry_in_s);
}

void test_compose_auth_fills_aid_short() {
  StreamSnapshot snap;
  ScheduleSnapshot sched;
  PersistedMeta meta = baseMeta();
  RenderInput in =
      composeRenderInput(DisplayState::Auth, snap, sched, meta, kNow);
  TEST_ASSERT_EQUAL(DisplayState::Auth, in.state);
  // Currently a placeholder until OGD/HAFAS path persists the live AID
  // prefix. Non-empty is the contract.
  TEST_ASSERT_TRUE(in.auth_aid_short[0] != '\0');
}

void test_compose_normal_merges_slots() {
  StreamSnapshot snap;
  snap.api_ok = true;
  snap.stream[STREAM_58A_ATZ].slot[0] = makeRealtime(kNow + 300);
  ScheduleSnapshot sched;
  PersistedMeta meta = baseMeta();
  RenderInput in =
      composeRenderInput(DisplayState::Normal, snap, sched, meta, kNow);
  TEST_ASSERT_EQUAL(DisplayState::Normal, in.state);
  TEST_ASSERT_TRUE(in.snapshot.stream[STREAM_58A_ATZ].slot[0].valid);
  TEST_ASSERT_EQUAL_INT64(kNow + 300,
                          in.snapshot.stream[STREAM_58A_ATZ].slot[0].when);
}

// On a fetch failure the raw snapshot is empty, but composeRenderInput merges
// it with the schedule — so a scheduled departure still fills the slot rather
// than blanking to "--:--". This is the behavior that replaced the Stale
// screen's forced-empty render.
void test_compose_normal_falls_back_to_schedule() {
  StreamSnapshot empty; // fetch failed → no realtime
  ScheduleSnapshot sched;
  sched.fetched_at = kNow - 3600;
  sched.hint[STREAM_58A_ATZ].first_tomorrow[0] = kNow + 600;
  PersistedMeta meta = baseMeta();
  RenderInput in =
      composeRenderInput(DisplayState::Normal, empty, sched, meta, kNow);
  TEST_ASSERT_EQUAL(DisplayState::Normal, in.state);
  TEST_ASSERT_TRUE(in.snapshot.stream[STREAM_58A_ATZ].slot[0].valid);
  TEST_ASSERT_EQUAL_INT64(kNow + 600,
                          in.snapshot.stream[STREAM_58A_ATZ].slot[0].when);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_state_auth_when_auth_error_seen);
  RUN_TEST(test_state_boot_when_first_render_ever);
  RUN_TEST(test_state_offline_when_wifi_down_and_stale);
  RUN_TEST(test_state_normal_when_long_since_success);
  RUN_TEST(test_state_normal_when_next_departure_far_out);
  RUN_TEST(test_state_normal_overnight_schedule_only);
  RUN_TEST(test_state_normal_when_realtime_imminent);
  RUN_TEST(test_state_normal_when_no_data_at_all);
  RUN_TEST(test_state_auth_dominates_boot);
  RUN_TEST(test_compose_boot_carries_version_string);
  RUN_TEST(test_compose_offline_fills_retry_in_s);
  RUN_TEST(test_compose_offline_clamps_retry_at_zero);
  RUN_TEST(test_compose_auth_fills_aid_short);
  RUN_TEST(test_compose_normal_merges_slots);
  RUN_TEST(test_compose_normal_falls_back_to_schedule);
  return UNITY_END();
}
