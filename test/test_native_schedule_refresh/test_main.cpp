// Tests for needScheduleRefresh + applyScheduleFetchResult. Pure
// host-side logic, no clock/network involvement — we feed wall-clock
// timestamps directly.

#include "logic/schedule_refresh.h"
#include "logic/slot_merger.h" // SCHEDULE_HINT_MAX_AGE_S

#include <ctime>
#include <unity.h>

using namespace bustaferl;

namespace {

// 2026-05-18 12:00:00 UTC. mktime() interprets struct tm as local time, so
// these tests run under whatever TZ Unity is invoked with. The relative
// arithmetic in needScheduleRefresh is TZ-stable as long as we compute
// `now` and `midnight` the same way the implementation does.
time_t makeLocal(int year, int mon, int mday, int hour, int min, int sec) {
  struct tm t {};
  t.tm_year = year - 1900;
  t.tm_mon = mon - 1;
  t.tm_mday = mday;
  t.tm_hour = hour;
  t.tm_min = min;
  t.tm_sec = sec;
  t.tm_isdst = -1;
  return mktime(&t);
}

} // namespace

// ===== needScheduleRefresh =====

void test_need_refresh_when_never_fetched() {
  ScheduleSnapshot s;
  // fetched_at default-initialised to 0.
  TEST_ASSERT_TRUE(needScheduleRefresh(s, makeLocal(2026, 5, 18, 12, 0, 0)));
}

void test_need_refresh_when_older_than_48h() {
  ScheduleSnapshot s;
  time_t now = makeLocal(2026, 5, 18, 12, 0, 0);
  s.fetched_at = now - SCHEDULE_HINT_MAX_AGE_S;
  TEST_ASSERT_TRUE(needScheduleRefresh(s, now));
}

void test_no_refresh_when_fresh_and_today() {
  ScheduleSnapshot s;
  time_t now = makeLocal(2026, 5, 18, 12, 0, 0);
  // Fetched 1h ago, today.
  s.fetched_at = now - 3600;
  TEST_ASSERT_FALSE(needScheduleRefresh(s, now));
}

void test_no_refresh_when_fetched_after_midnight_even_if_last_today_passed() {
  // Even if last_today is in the past, a fetch that already happened past
  // today's local midnight is fresh enough for this service day.
  ScheduleSnapshot s;
  time_t now = makeLocal(2026, 5, 18, 23, 0, 0);
  s.fetched_at = makeLocal(2026, 5, 18, 6, 0, 0); // this morning
  s.hint[STREAM_58A_ATZ].last_today = makeLocal(2026, 5, 18, 22, 0, 0);
  TEST_ASSERT_FALSE(needScheduleRefresh(s, now));
}

void test_refresh_when_last_today_passed_and_fetched_yesterday() {
  ScheduleSnapshot s;
  time_t now = makeLocal(2026, 5, 18, 23, 0, 0);
  // Fetched the previous evening (before today's local midnight).
  s.fetched_at = makeLocal(2026, 5, 17, 22, 0, 0);
  s.hint[STREAM_58A_ATZ].last_today = makeLocal(2026, 5, 18, 22, 0, 0);
  TEST_ASSERT_TRUE(needScheduleRefresh(s, now));
}

void test_no_refresh_when_last_today_still_in_future() {
  ScheduleSnapshot s;
  time_t now = makeLocal(2026, 5, 18, 12, 0, 0);
  s.fetched_at = makeLocal(2026, 5, 17, 22, 0, 0);
  s.hint[STREAM_58A_ATZ].last_today = makeLocal(2026, 5, 18, 22, 0, 0);
  TEST_ASSERT_FALSE(needScheduleRefresh(s, now));
}

// ===== applyScheduleFetchResult =====

void test_apply_not_ok_returns_false_and_leaves_snapshot_untouched() {
  ScheduleSnapshot before;
  before.fetched_at = 12345;
  before.hint[STREAM_58A_ATZ].first_tomorrow[0] = 99999;

  ScheduleSnapshot s = before;
  ScheduleFetchResult r;
  r.ok = false;
  r.hint[STREAM_58A_ATZ].first_tomorrow[0] = 11111;

  TEST_ASSERT_FALSE(applyScheduleFetchResult(r, 67890, s));
  TEST_ASSERT_EQUAL_INT64(before.fetched_at, s.fetched_at);
  TEST_ASSERT_EQUAL_INT64(before.hint[STREAM_58A_ATZ].first_tomorrow[0],
                          s.hint[STREAM_58A_ATZ].first_tomorrow[0]);
}

void test_apply_ok_stamps_fetched_at() {
  ScheduleSnapshot s;
  ScheduleFetchResult r;
  r.ok = true;
  TEST_ASSERT_TRUE(applyScheduleFetchResult(r, 67890, s));
  TEST_ASSERT_EQUAL_INT64(67890, s.fetched_at);
}

void test_apply_only_overwrites_streams_with_real_data() {
  ScheduleSnapshot s;
  s.hint[STREAM_58A_ATZ].first_tomorrow[0] = 100;
  s.hint[STREAM_58B_ATZ].first_tomorrow[0] = 200;

  ScheduleFetchResult r;
  r.ok = true;
  // Stream 58A got fresh data; stream 58B's call failed (zero sentinel).
  r.hint[STREAM_58A_ATZ].first_tomorrow[0] = 999;
  // Stream 58B left as zero-init — must NOT overwrite the existing 200.

  TEST_ASSERT_TRUE(applyScheduleFetchResult(r, 555, s));
  TEST_ASSERT_EQUAL_INT64(999, s.hint[STREAM_58A_ATZ].first_tomorrow[0]);
  TEST_ASSERT_EQUAL_INT64(200, s.hint[STREAM_58B_ATZ].first_tomorrow[0]);
}

void test_apply_overwrites_when_only_last_today_set() {
  // A stream where last_today is set but first_tomorrow is still zero
  // (e.g. last EFA call landed late and the entries were all today). The
  // partial result must still be applied to that stream.
  ScheduleSnapshot s;
  s.hint[STREAM_58A_ATZ].last_today = 100;

  ScheduleFetchResult r;
  r.ok = true;
  r.hint[STREAM_58A_ATZ].last_today = 999;

  TEST_ASSERT_TRUE(applyScheduleFetchResult(r, 555, s));
  TEST_ASSERT_EQUAL_INT64(999, s.hint[STREAM_58A_ATZ].last_today);
}

void test_apply_overwrites_when_only_next_today_set() {
  // Schritt 2.3 edge: parser may write next_today without last_today (the
  // rolling window logic shares the same pre-cutoff entries, but a future
  // refactor could decouple them). Verify the apply gate accepts that case
  // so a fresh stream isn't silently discarded.
  ScheduleSnapshot s;
  s.hint[STREAM_58A_ATZ].first_tomorrow[0] = 100;

  ScheduleFetchResult r;
  r.ok = true;
  r.hint[STREAM_58A_ATZ].next_today[1] = 777;

  TEST_ASSERT_TRUE(applyScheduleFetchResult(r, 555, s));
  TEST_ASSERT_EQUAL_INT64(777, s.hint[STREAM_58A_ATZ].next_today[1]);
  TEST_ASSERT_EQUAL_INT64(0, s.hint[STREAM_58A_ATZ].first_tomorrow[0]);
}

void setUp() {}
void tearDown() {}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_need_refresh_when_never_fetched);
  RUN_TEST(test_need_refresh_when_older_than_48h);
  RUN_TEST(test_no_refresh_when_fresh_and_today);
  RUN_TEST(
      test_no_refresh_when_fetched_after_midnight_even_if_last_today_passed);
  RUN_TEST(test_refresh_when_last_today_passed_and_fetched_yesterday);
  RUN_TEST(test_no_refresh_when_last_today_still_in_future);
  RUN_TEST(test_apply_not_ok_returns_false_and_leaves_snapshot_untouched);
  RUN_TEST(test_apply_ok_stamps_fetched_at);
  RUN_TEST(test_apply_only_overwrites_streams_with_real_data);
  RUN_TEST(test_apply_overwrites_when_only_last_today_set);
  RUN_TEST(test_apply_overwrites_when_only_next_today_set);
  return UNITY_END();
}
