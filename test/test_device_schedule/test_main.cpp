// On-device test for the EFA schedule pipeline. Reproduces the exact
// cold-boot sequence that was crashing with abort() during the third EFA
// fetch: WiFi up -> NTP sync -> three sequential XSLT_DM_REQUEST calls.
// Logs free heap at every step so the regression of "heap exhausted before
// 3rd call" is visible in the Unity report instead of disappearing into a
// reboot loop.

#include <Arduino.h>
#include <esp_system.h>
#include <unity.h>

#include "config.h"
#include "data/ScheduleHint.h"
#include "data/efa_parse.h"
#include "hal/Esp32Clock.h"
#include "hal/Esp32Network.h"
#include "logic/schedule_fetcher.h"
#include "secrets.h"

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

void buildScheduleFilters(ScheduleStreamFilter (&f)[STREAM_COUNT]) {
  f[STREAM_58A_ATZ] = {DIVA_TULLNERTALGASSE, LINE_58A, EFA_TOWARDS_58A_ATZ};
  f[STREAM_58A_HIETZING] = {DIVA_TULLNERTALGASSE, LINE_58A,
                            EFA_TOWARDS_58A_HIETZING};
  f[STREAM_58B_ATZ] = {DIVA_ENDEMANNGASSE, LINE_58B, EFA_TOWARDS_58B_ATZ};
  f[STREAM_U1_LEOPOLDAU] = {DIVA_SUEDTIROLER_PLATZ, LINE_U1,
                            EFA_TOWARDS_U1_LEOPOLDAU};
  f[STREAM_U1_OBERLAA] = {DIVA_SUEDTIROLER_PLATZ, LINE_U1,
                          EFA_TOWARDS_U1_OBERLAA};
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
  TEST_ASSERT_GREATER_THAN_MESSAGE(1700000000, t, "clock not synced");
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
  TEST_ASSERT_TRUE_MESSAGE(g_net.httpGet(url, body), "single EFA GET failed");
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
  // The whole production sequence: three DIVAs back-to-back. This is the
  // exact path that was rebooting the device. Pass = we got here without an
  // abort, plus we collected at least some hint data.
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

  TEST_ASSERT_EQUAL_INT_MESSAGE(3, r.calls_attempted,
                                "expected 3 DIVA calls (one per Haltestelle)");
  TEST_ASSERT_EQUAL_INT_MESSAGE(
      0, r.calls_failed,
      "at least one EFA call failed — heap guard skipped or HTTP error");
  TEST_ASSERT_TRUE_MESSAGE(r.ok, "fetchSchedule reported not ok");

  // At least one stream must have collected a first_tomorrow value — the
  // happy path returns the morning departures even outside service hours.
  bool any_hint = false;
  for (int i = 0; i < STREAM_COUNT; ++i) {
    if (r.hint[i].first_tomorrow[0] != 0) {
      any_hint = true;
      break;
    }
  }
  TEST_ASSERT_TRUE_MESSAGE(any_hint, "no stream got a first_tomorrow hint — "
                                     "EFA direction string mismatch?");
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
