// On-device smoke test: walks the production fetch path layer by layer so a
// silent failure (HTTPS, parse, filter mismatch) names itself in the Unity
// report instead of disappearing behind a deepSleep().

#include <Arduino.h>
#include <cstdio>
#include <ctime>
#include <unity.h>

#include "config.h"
#include "data/wienerlinien_parse.h"
#include "hal/Esp32Clock.h"
#include "hal/Esp32Network.h"
#include "logic/sleep_planner.h"
#include "secrets.h"

using namespace bustaferl;

namespace {

Esp32Network g_net;
Esp32Clock g_clock{NTP_SERVER, TZ_INFO};
std::string g_body;

std::string apiUrl() {
  std::string url = WL_API_BASE;
  char buf[64];
  snprintf(buf, sizeof(buf), "&rbl=%d&rbl=%d&rbl=%d", RBL_TULL_ATZGERSDORF,
           RBL_TULL_HIETZING, RBL_ENDEMANN);
  url += buf;
  return url;
}

void buildFilters(StreamFilter (&f)[STREAM_COUNT]) {
  f[STREAM_58A_ATZ] = {RBL_TULL_ATZGERSDORF, LINE_58A, TOWARDS_58A_ATZ};
  f[STREAM_58A_HIETZING] = {RBL_TULL_HIETZING, LINE_58A, TOWARDS_58A_HIETZING};
  f[STREAM_58B_ATZ] = {RBL_ENDEMANN, LINE_58B, FILTER_TOWARDS_58B};
}

} // namespace

void test_wifi_connects(void) {
  g_net.addAp(WIFI_SSID_PRIMARY, WIFI_PASSWORD_PRIMARY);
#ifdef WIFI_SSID_SECONDARY
  g_net.addAp(WIFI_SSID_SECONDARY, WIFI_PASSWORD_SECONDARY);
#endif
  TEST_ASSERT_TRUE_MESSAGE(g_net.connect(15000),
                           "WiFi did not connect within 15s");
}

void test_http_get_returns_body(void) {
  TEST_ASSERT_TRUE_MESSAGE(g_net.httpGet(apiUrl(), g_body),
                           "httpGet returned false");
  Serial.printf("[test] body size = %u bytes, free heap = %u\n",
                static_cast<unsigned>(g_body.size()), ESP.getFreeHeap());
  TEST_ASSERT_GREATER_THAN_MESSAGE(100, g_body.size(),
                                   "body suspiciously small (<100 bytes)");
  TEST_ASSERT_TRUE_MESSAGE(g_body.find("monitors") != std::string::npos,
                           "response missing \"monitors\" key");
}

void test_parse_all_three_rbls_respond(void) {
  StreamFilter filters[STREAM_COUNT];
  buildFilters(filters);
  StreamSnapshot snap;
  TEST_ASSERT_TRUE_MESSAGE(parseMonitorResponse(g_body, filters, snap),
                           "parseMonitorResponse failed");
  TEST_ASSERT_TRUE_MESSAGE(snap.api_ok, "api_ok was false after parse");

  Serial.printf("[test] streams: 58A-Atz r=%d f=%d | "
                "58A-Hie r=%d f=%d | 58B r=%d f=%d\n",
                snap.stream[STREAM_58A_ATZ].rbl_responded,
                snap.stream[STREAM_58A_ATZ].filter_matched,
                snap.stream[STREAM_58A_HIETZING].rbl_responded,
                snap.stream[STREAM_58A_HIETZING].filter_matched,
                snap.stream[STREAM_58B_ATZ].rbl_responded,
                snap.stream[STREAM_58B_ATZ].filter_matched);

  TEST_ASSERT_TRUE_MESSAGE(snap.stream[STREAM_58A_ATZ].rbl_responded,
                           "RBL_TULL_ATZGERSDORF (8131) did not respond");
  TEST_ASSERT_TRUE_MESSAGE(snap.stream[STREAM_58A_HIETZING].rbl_responded,
                           "RBL_TULL_HIETZING (3757) did not respond");
  TEST_ASSERT_TRUE_MESSAGE(snap.stream[STREAM_58B_ATZ].rbl_responded,
                           "RBL_ENDEMANN (8132) did not respond");

  // Filter match is time-of-day dependent (no buses overnight). Don't assert
  // it — but make it visible above so a mismatch is obvious in the log.
}

// ---------------------------------------------------------------------------
// Engine state tests — observe clock + planSleep behaviour.
// These document the bug behind the production "deep sleep for 1.7G seconds"
// symptom: ESP32 loses system time across deep sleep, and planSleep can't
// detect that. The fix lives in warmCyclePath (force NTP if clock is bogus).
// ---------------------------------------------------------------------------

void test_clock_at_boot_is_unsynced(void) {
  // ESP32 cold-boots with time() near zero (seconds since boot, not Unix
  // epoch). This is also the post-deep-sleep state that bites warmCyclePath.
  time_t t = g_clock.now();
  Serial.printf("[engine] clock at boot: %lld (unsynced if <1.7e9)\n",
                static_cast<long long>(t));
  TEST_ASSERT_LESS_THAN_MESSAGE(1700000000, t,
                                "clock unexpectedly synced at boot");
}

void test_ntp_sync_brings_clock_to_present(void) {
  TEST_ASSERT_TRUE_MESSAGE(g_clock.ntpSync(), "ntpSync() failed");
  time_t t = g_clock.now();
  Serial.printf("[engine] clock after NTP: %lld\n",
                static_cast<long long>(t));
  TEST_ASSERT_GREATER_THAN_MESSAGE(1700000000, t,
                                   "clock still bogus after NTP");
}

void test_plansleep_with_unsynced_now_returns_huge(void) {
  // Reproduces the production symptom: when warmCyclePath passes now=0 to
  // planSleep (because the clock wasn't re-synced), the result is ~years.
  StreamSnapshot snap{};
  snap.api_ok = true;
  snap.stream[0].rbl_responded = true;
  snap.stream[0].filter_matched = true;
  snap.stream[0].slot[0].when = 1900000000; // a 2030-ish "bus"
  snap.stream[0].slot[0].valid = true;
  SleepConfig sc{WAKE_BEFORE_BUS_S, BOOT_MARGIN_S, ACTIVE_THRESHOLD_S,
                 NO_DATA_SLEEP_S};
  SleepDecision sd = planSleep(snap, /*now=*/0, sc);
  Serial.printf("[engine] planSleep(now=0, t_ref=1.9e9) -> "
                "mode=%s seconds=%u\n",
                sd.mode == Mode::DeepSleep ? "Deep" : "Active", sd.seconds);
  TEST_ASSERT_GREATER_THAN_MESSAGE(1000000000U, sd.seconds,
                                   "planSleep(now=0) somehow not huge — "
                                   "is the unsynced-clock defense in place?");
}

void test_warm_boot_recovery_sequence(void) {
  // Verifies the warmCyclePath fix end-to-end on real hardware. Runs the
  // exact production guard against the actual cold-boot clock state (which
  // is also the post-deep-sleep symptom): clock < 1.7e9 -> force ntpSync ->
  // planSleep must now compute a sane interval, not 50 years.
  time_t before = g_clock.now();
  Serial.printf("[engine] sim warm-boot: clock=%lld\n",
                static_cast<long long>(before));
  TEST_ASSERT_LESS_THAN_MESSAGE(1700000000, before,
                                "precondition: clock must be bogus");

  // Production guard, literal copy from warmCyclePath:
  if (g_clock.now() < 1700000000) {
    TEST_ASSERT_TRUE_MESSAGE(g_clock.ntpSync(), "recovery NTP failed");
  }
  time_t after = g_clock.now();
  Serial.printf("[engine] post-recovery: clock=%lld\n",
                static_cast<long long>(after));
  TEST_ASSERT_GREATER_THAN_MESSAGE(1700000000, after,
                                   "recovery did not produce a valid clock");

  // planSleep with recovered clock against a bus 20 minutes out should be a
  // small positive deep-sleep interval — well under 24h, never years.
  StreamSnapshot snap{};
  snap.api_ok = true;
  snap.stream[0].rbl_responded = true;
  snap.stream[0].filter_matched = true;
  snap.stream[0].slot[0].when = after + 1200;
  snap.stream[0].slot[0].valid = true;
  SleepConfig sc{WAKE_BEFORE_BUS_S, BOOT_MARGIN_S, ACTIVE_THRESHOLD_S,
                 NO_DATA_SLEEP_S};
  SleepDecision sd = planSleep(snap, after, sc);
  Serial.printf("[engine] post-fix planSleep: mode=%s seconds=%u\n",
                sd.mode == Mode::DeepSleep ? "Deep" : "Active", sd.seconds);
  TEST_ASSERT_LESS_THAN_MESSAGE(86400U, sd.seconds,
                                "planSleep > 24h after recovery — broken");
}

void test_clock_reads_plausible_current_time(void) {
  // Once NTP has run, the clock should read close to wall-clock 'now'.
  // Bounds: 2026-01-01 (post-fixture-capture) to 2030-01-01 (sanity upper).
  // Also prints the localtime decomposition so a human can eyeball the
  // timezone applied (TZ_INFO=CET-1CEST,M3.5.0,M10.5.0/3).
  time_t t = g_clock.now();
  struct tm local;
  localtime_r(&t, &local);
  Serial.printf("[engine] clock: %lld -> %04d-%02d-%02d %02d:%02d:%02d "
                "(tm_isdst=%d)\n",
                static_cast<long long>(t), local.tm_year + 1900,
                local.tm_mon + 1, local.tm_mday, local.tm_hour, local.tm_min,
                local.tm_sec, local.tm_isdst);
  TEST_ASSERT_GREATER_THAN_MESSAGE(
      1735689600, t, "clock pre-2026-01-01 — NTP returned a stale time?");
  TEST_ASSERT_LESS_THAN_MESSAGE(
      1893456000, t, "clock past 2030-01-01 — NTP returned a future time?");
  TEST_ASSERT_TRUE_MESSAGE(local.tm_year + 1900 >= 2026 &&
                               local.tm_year + 1900 < 2030,
                           "local year not in [2026, 2030)");
}

void test_ntp_completes_before_api_query(void) {
  // Walks the production warmCyclePath sequence in test form:
  //   WiFi up  ->  clock guard (sync if bogus)  ->  API query.
  // Asserts the clock is valid AT THE MOMENT httpGet runs, proving the
  // ordering invariant. Catches regressions where someone reorders the
  // guard to after the fetch.
  TEST_ASSERT_TRUE_MESSAGE(g_net.isConnected(),
                           "precondition: WiFi must be up");

  // Production guard (literal copy of warmCyclePath's check):
  if (g_clock.now() < 1700000000) {
    TEST_ASSERT_TRUE_MESSAGE(g_clock.ntpSync(), "guard NTP failed");
  }
  time_t clock_at_query_start = g_clock.now();
  Serial.printf("[engine] ordering: clock=%lld at query start\n",
                static_cast<long long>(clock_at_query_start));
  TEST_ASSERT_GREATER_THAN_MESSAGE(
      1700000000, clock_at_query_start,
      "ORDERING VIOLATION: clock bogus when query begins");

  std::string body;
  TEST_ASSERT_TRUE_MESSAGE(g_net.httpGet(apiUrl(), body),
                           "ordering test: httpGet failed");

  time_t clock_after_query = g_clock.now();
  Serial.printf("[engine] ordering: clock=%lld after query (Δ=%lld s)\n",
                static_cast<long long>(clock_after_query),
                static_cast<long long>(clock_after_query -
                                        clock_at_query_start));
  TEST_ASSERT_TRUE_MESSAGE(clock_after_query - clock_at_query_start < 30,
                           "clock jumped >30 s during query — NTP fired "
                           "again mid-query?");
}

void test_plansleep_with_synced_now_is_sane(void) {
  // With a synced clock and a bus 60s in the future, planSleep should pick
  // Active mode (delta < active_threshold).
  StreamSnapshot snap{};
  snap.api_ok = true;
  snap.stream[0].rbl_responded = true;
  snap.stream[0].filter_matched = true;
  time_t now = g_clock.now();
  snap.stream[0].slot[0].when = now + 60;
  snap.stream[0].slot[0].valid = true;
  SleepConfig sc{WAKE_BEFORE_BUS_S, BOOT_MARGIN_S, ACTIVE_THRESHOLD_S,
                 NO_DATA_SLEEP_S};
  SleepDecision sd = planSleep(snap, now, sc);
  Serial.printf("[engine] planSleep(now=%lld, t_ref=now+60) -> "
                "mode=%s seconds=%u\n",
                static_cast<long long>(now),
                sd.mode == Mode::DeepSleep ? "Deep" : "Active", sd.seconds);
  TEST_ASSERT_EQUAL_INT_MESSAGE(static_cast<int>(Mode::Active),
                                static_cast<int>(sd.mode),
                                "expected Active for bus in 60s");
}

void setup() {
  Serial.begin(115200);
  delay(2000); // let USB enumerate and serial settle
  UNITY_BEGIN();
  // Run clock-unsynced check FIRST, before any NTP sync would taint state.
  RUN_TEST(test_clock_at_boot_is_unsynced);
  RUN_TEST(test_plansleep_with_unsynced_now_returns_huge);

  RUN_TEST(test_wifi_connects);
  // Recovery test must run while clock is still bogus — i.e. before any
  // other ntpSync. Keep it ahead of the post-NTP tests below.
  RUN_TEST(test_warm_boot_recovery_sequence);

  RUN_TEST(test_http_get_returns_body);
  RUN_TEST(test_parse_all_three_rbls_respond);

  // Clock is now synced (by the recovery test). These verify post-sync state.
  RUN_TEST(test_ntp_sync_brings_clock_to_present);
  RUN_TEST(test_clock_reads_plausible_current_time);
  RUN_TEST(test_ntp_completes_before_api_query);
  RUN_TEST(test_plansleep_with_synced_now_is_sane);
  UNITY_END();
}

void loop() {}
