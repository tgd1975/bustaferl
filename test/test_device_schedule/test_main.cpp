// On-device test for the EFA schedule pipeline. Reproduces the cold-boot
// sequence that historically crashed with abort() during back-to-back EFA
// fetches: WiFi up -> NTP sync -> distinct XSLT_DM_REQUEST calls.
// Logs free heap at every step so a "heap exhausted before final call"
// regression is visible in the Unity report instead of disappearing into a
// reboot loop.
//
// v2 (Schritt 8.3): the S-Bahn stream has no EFA hint path (Variante 1) —
// `buildScheduleFilters` leaves diva=0, the fetcher's skip-guard takes it,
// and the prod call count drops from 3 distinct DIVAs to 2.

#include "config.h"
#include "data/ScheduleHint.h"
#include "data/efa_parse.h"
#include "hal/Esp32Clock.h"
#include "hal/Esp32Network.h"
#include "logic/filter_builder.h"
#include "logic/schedule_fetcher.h"
#include "secrets.h"

#include <Arduino.h>
#include <cstdio>
#include <esp_system.h>
#include <unity.h>

using namespace bustaferl;

namespace {

Esp32Network g_net;
Esp32Clock g_clock{NTP_SERVER, TZ_INFO};

void logHeap(const char *tag) {
  Serial.printf("[heap] %-24s free=%u largest=%u min_free=%u\n", tag,
                static_cast<unsigned>(esp_get_free_heap_size()),
                static_cast<unsigned>(
                    heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT)),
                static_cast<unsigned>(esp_get_minimum_free_heap_size()));
}

} // namespace

void test_wifi_connects() {
  g_net.addAp(WIFI_SSID_PRIMARY, WIFI_PASSWORD_PRIMARY);
#ifdef WIFI_SSID_SECONDARY
  g_net.addAp(WIFI_SSID_SECONDARY, WIFI_PASSWORD_SECONDARY);
#endif
  logHeap("pre-wifi");
  TEST_ASSERT_TRUE_MESSAGE(g_net.connect(15000), "WiFi did not connect in 15s");
  logHeap("post-wifi");
}

void test_ntp_syncs() {
  TEST_ASSERT_TRUE_MESSAGE(g_clock.ntpSync(), "ntpSync failed");
  time_t t = g_clock.now();
  Serial.printf("[ntp] now=%lld\n", static_cast<long long>(t));
  TEST_ASSERT_GREATER_THAN_MESSAGE(MIN_PLAUSIBLE_EPOCH, t, "clock not synced");
}

void test_single_efa_call_does_not_crash() {
  // First EFA call in isolation. If this alone OOMs we know the streaming
  // httpGet didn't shrink memory enough.
  ScheduleStreamFilter f[STREAM_COUNT];
  buildScheduleFilters(f);
  ScheduleFetchConfig cfg;
  cfg.endpoint_base = WL_EFA_DM_BASE;

  // Build the URL the production fetcher would use for just DIVA #1.
  time_t now = g_clock.now();
  time_t query_time;
  {
    struct tm local;
    localtime_r(&now, &local);
    local.tm_hour = cfg.query_hour;
    local.tm_min = cfg.query_minute;
    local.tm_sec = 0;
    local.tm_isdst = -1;
    query_time = mktime(&local);
  }
  std::string url = buildEfaUrl(cfg.endpoint_base, DIVA_TULLNERTALGASSE,
                                query_time, cfg.limit);
  logHeap("pre-single-call");
  std::string body;
  {
    auto _r = g_net.httpGet(url, body);
    TEST_ASSERT_TRUE_MESSAGE(_r.ok && _r.http_status >= 200 &&
                                 _r.http_status < 300,
                             "single EFA GET non-2xx");
  }
  Serial.printf("[efa] single body=%u bytes\n",
                static_cast<unsigned>(body.size()));
  logHeap("post-single-call");
  TEST_ASSERT_GREATER_THAN_MESSAGE(10000, body.size(),
                                   "EFA body suspiciously small");

  // Parse it once to flex the filter path under real response shape.
  ScheduleHint hint[STREAM_COUNT]{};
  time_t cutoff = computeCutoff(now, cfg.cutoff_hour);
  TEST_ASSERT_TRUE_MESSAGE(
      parseEfaResponse(body, DIVA_TULLNERTALGASSE, f, cutoff, hint),
      "single-call parse failed");
  logHeap("post-single-parse");
}

void test_full_schedule_fetch_does_not_crash() {
  // The whole production sequence: two DIVAs back-to-back (Tullnertalgasse +
  // Endemanngasse — the S-Bahn stream has no EFA hint path in v2, so the
  // fetcher's diva-skip-guard takes it). Pass = we got here without an
  // abort, plus the bus streams collected hint data.
  ScheduleStreamFilter f[STREAM_COUNT];
  buildScheduleFilters(f);
  ScheduleFetchConfig cfg;
  cfg.endpoint_base = WL_EFA_DM_BASE;

  logHeap("pre-full-fetch");
  ScheduleFetchResult r = fetchSchedule(g_net, g_clock.now(), f, cfg);
  logHeap("post-full-fetch");

  Serial.printf("[efa] full result ok=%d attempted=%d failed=%d\n", r.ok,
                r.calls_attempted, r.calls_failed);
  for (int i = 0; i < STREAM_COUNT; ++i) {
    Serial.printf(
        "[efa] stream %d last_today=%lld first[0]=%lld first[1]=%lld\n", i,
        static_cast<long long>(r.hint[i].last_today),
        static_cast<long long>(r.hint[i].first_tomorrow[0]),
        static_cast<long long>(r.hint[i].first_tomorrow[1]));
  }

  TEST_ASSERT_EQUAL_INT_MESSAGE(
      2, r.calls_attempted,
      "expected 2 distinct DIVA calls (Tullnertalgasse + Endemanngasse — "
      "S-Bahn has no EFA hint path in v2)");
  TEST_ASSERT_EQUAL_INT_MESSAGE(
      0, r.calls_failed,
      "at least one EFA call failed — heap guard skipped or HTTP error");
  TEST_ASSERT_TRUE_MESSAGE(r.ok, "fetchSchedule reported not ok");

  // The three bus streams must each collect at least one schedule hint:
  // either a last_today (any departure before tomorrow's 03:00 cutoff) or
  // a first_tomorrow (departures after the cutoff). Mid-day runs see only
  // last_today because the default EFA window (limit=50, anchored at
  // 22:00) typically doesn't span past 03:00; late-evening runs see both.
  // A stream with NEITHER is a direction-filter mismatch.
  // STREAM_SBAHN_HBF is skipped on purpose (diva=0) — it must stay empty.
  for (int i = 0; i < STREAM_COUNT; ++i) {
    const bool any_hint =
        r.hint[i].last_today != 0 || r.hint[i].first_tomorrow[0] != 0;
    char msg[96];
    if (i == STREAM_SBAHN_HBF) {
      std::snprintf(msg, sizeof(msg),
                    "S-Bahn stream got a hint — diva-skip-guard regression?");
      TEST_ASSERT_FALSE_MESSAGE(any_hint, msg);
    } else {
      std::snprintf(msg, sizeof(msg),
                    "stream %d got no hint at all — EFA direction mismatch?",
                    i);
      TEST_ASSERT_TRUE_MESSAGE(any_hint, msg);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  UNITY_BEGIN();
  RUN_TEST(test_wifi_connects);
  RUN_TEST(test_ntp_syncs);
  RUN_TEST(test_single_efa_call_does_not_crash);
  RUN_TEST(test_full_schedule_fetch_does_not_crash);
  UNITY_END();
}

void loop() {}
