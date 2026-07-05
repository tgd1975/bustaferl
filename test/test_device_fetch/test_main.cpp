// On-device smoke test: walks the production fetch path layer by layer so a
// silent failure (HTTPS, parse, filter mismatch) names itself in the Unity
// report instead of disappearing behind a deepSleep().
//
// v2 (Schritt 8.3): pipeline now has two endpoints — Wiener Linien OGD
// monitor for the 3 bus streams and ÖBB HAFAS mgate.exe for the S-Bahn
// stream. Both are exercised here. Bus streams use the prod URL/filter
// helpers directly (`apiUrlForBatch`, `buildStreamFilters`); the HAFAS path
// runs through `fetchSnapshot` end-to-end so the full integration plus
// auth-tripwire bookkeeping is on the critical path.

#include "config.h"
#include "data/oebb_hafas_parse.h"
#include "data/wienerlinien_parse.h"
#include "hal/Esp32Clock.h"
#include "hal/Esp32Network.h"
#include "hal/IPersistentStore.h"
#include "logic/filter_builder.h"
#include "logic/sleep_planner.h"
#include "logic/snapshot_fetcher.h"
#include "secrets.h"

#include <Arduino.h>
#include <cstdio>
#include <ctime>
#include <unity.h>

using namespace bustaferl;

namespace {

Esp32Network g_net;
Esp32Clock g_clock{NTP_SERVER, TZ_INFO};
std::string g_body;

void printClock(const char *tag) {
  time_t t = g_clock.now();
  struct tm local;
  localtime_r(&t, &local);
  Serial.printf("[ntp] %s epoch=%lld local=%04d-%02d-%02d %02d:%02d:%02d "
                "isdst=%d\n",
                tag, static_cast<long long>(t), local.tm_year + 1900,
                local.tm_mon + 1, local.tm_mday, local.tm_hour, local.tm_min,
                local.tm_sec, local.tm_isdst);
}

void printSlot(const char *stream, const Departure &d) {
  if (!d.valid) {
    Serial.printf("[api] %s slot=--:-- (invalid)\n", stream);
    return;
  }
  struct tm local;
  localtime_r(&d.when, &local);
  Serial.printf("[api] %s slot=%02d:%02d %s line=%-3s epoch=%lld\n", stream,
                local.tm_hour, local.tm_min,
                d.source == DepartureSource::Realtime ? "RT" : "PLAN",
                d.line_label, static_cast<long long>(d.when));
}

// Build a single-batch OGD probe URL with all three bus stopIds. Production
// splits these into batches of two (STOPIDS_PER_QUERY=2), but for a one-shot
// reachability check a single URL is simpler and exercises the same parser.
std::string ogdProbeUrl() {
  std::string url = WL_API_BASE;
  char buf[96];
  std::snprintf(buf, sizeof(buf), "&stopId=%d&stopId=%d&stopId=%d",
                RBL_TULL_ATZGERSDORF, RBL_TULL_HIETZING, RBL_ENDEMANN);
  url += buf;
  return url;
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
  Serial.printf("[api] GET %s\n", ogdProbeUrl().c_str());
  {
    auto _r = g_net.httpGet(ogdProbeUrl(), g_body);
    TEST_ASSERT_TRUE_MESSAGE(_r.ok && _r.http_status >= 200 &&
                                 _r.http_status < 300,
                             "httpGet returned non-2xx");
  }
  Serial.printf("[api] body size = %u bytes, free heap = %u\n",
                static_cast<unsigned>(g_body.size()), ESP.getFreeHeap());
  // Print a head + tail snippet so a malformed/truncated response is obvious
  // in the log without dumping the entire payload over serial.
  const size_t head = g_body.size() < 240 ? g_body.size() : 240;
  Serial.printf("[api] body head: %.*s\n", static_cast<int>(head),
                g_body.c_str());
  if (g_body.size() > 480) {
    Serial.printf("[api] body tail: %s\n",
                  g_body.c_str() + g_body.size() - 240);
  }
  TEST_ASSERT_GREATER_THAN_MESSAGE(100, g_body.size(),
                                   "body suspiciously small (<100 bytes)");
  TEST_ASSERT_TRUE_MESSAGE(g_body.find("monitors") != std::string::npos,
                           "response missing \"monitors\" key");
}

void test_parse_all_bus_streams_respond(void) {
  StreamFilter filters[STREAM_COUNT];
  buildStreamFilters(filters);
  StreamSnapshot snap;
  TEST_ASSERT_TRUE_MESSAGE(parseMonitorResponse(g_body, filters, snap),
                           "parseMonitorResponse failed");
  TEST_ASSERT_TRUE_MESSAGE(snap.api_ok, "api_ok was false after parse");

  Serial.printf("[api] streams: 58A-Atz r=%d f=%d | "
                "58A-Hie r=%d f=%d | 58B r=%d f=%d\n",
                snap.stream[STREAM_58A_ATZ].endpoint_responded,
                snap.stream[STREAM_58A_ATZ].filter_matched,
                snap.stream[STREAM_58A_HIETZING].endpoint_responded,
                snap.stream[STREAM_58A_HIETZING].filter_matched,
                snap.stream[STREAM_58B_ATZ].endpoint_responded,
                snap.stream[STREAM_58B_ATZ].filter_matched);

  // Per-stream parsed departure times — visible in serial so a wrong towards
  // filter (endpoint_responded but no slots) is obvious at a glance.
  for (int slot = 0; slot < SLOTS_PER_STREAM; ++slot) {
    char tag[24];
    std::snprintf(tag, sizeof(tag), "58A-Atz[%d]", slot);
    printSlot(tag, snap.stream[STREAM_58A_ATZ].slot[slot]);
    std::snprintf(tag, sizeof(tag), "58A-Hie[%d]", slot);
    printSlot(tag, snap.stream[STREAM_58A_HIETZING].slot[slot]);
    std::snprintf(tag, sizeof(tag), "58B-Atz[%d]", slot);
    printSlot(tag, snap.stream[STREAM_58B_ATZ].slot[slot]);
  }

  TEST_ASSERT_TRUE_MESSAGE(snap.stream[STREAM_58A_ATZ].endpoint_responded,
                           "RBL_TULL_ATZGERSDORF (8131) did not respond");
  TEST_ASSERT_TRUE_MESSAGE(snap.stream[STREAM_58A_HIETZING].endpoint_responded,
                           "RBL_TULL_HIETZING (3757) did not respond");
  TEST_ASSERT_TRUE_MESSAGE(snap.stream[STREAM_58B_ATZ].endpoint_responded,
                           "RBL_ENDEMANN (8132) did not respond");

  // S-Bahn stream is handled by the HAFAS path (next test) — it must stay
  // untouched by the OGD parse.
  TEST_ASSERT_FALSE_MESSAGE(snap.stream[STREAM_SBAHN_HBF].endpoint_responded,
                            "OGD parse unexpectedly touched the S-Bahn slot");

  // Filter match is time-of-day dependent (no buses overnight). Don't assert
  // it — but make it visible above so a mismatch is obvious in the log.
}

void test_fetchSnapshot_fills_oebb_stream(void) {
  // End-to-end: run the production `fetchSnapshot` which executes the OGD
  // batch loop AND the HAFAS mgate.exe POST. Asserts that the S-Bahn slot is
  // populated, line_label is non-empty (S2/S3/S4/REX vary per slot), and
  // that the auth-tripwire stays clear on a happy-path response.
  StreamFilter filters[STREAM_COUNT];
  buildStreamFilters(filters);
  OebbStreamFilter oebb_filter = buildOebbFilter();
  std::string ogd_base = WL_API_BASE;
  std::string mgate_url = OEBB_MGATE_URL;
  FetchInputs inputs{ogd_base, mgate_url, filters, oebb_filter};
  StreamSnapshot snap;
  FetchSummary summary;
  PersistedMeta meta{};
  Serial.printf("[api] fetchSnapshot start, free heap = %u\n",
                ESP.getFreeHeap());
  bool ok = fetchSnapshot(g_net, inputs, g_clock.now(), snap, summary, meta);
  Serial.printf("[api] fetchSnapshot ok=%d batches=%d failed=%d auth_seen=%d "
                "heap=%u\n",
                ok, summary.total_batches, summary.failed_batches,
                meta.auth_error_seen, ESP.getFreeHeap());

  // Schritt 9.3/9.4 — Heap- und Sleep-Budget pro Leg. Diese Zeilen sind das
  // Roh-Material für die Vergleichsläufe (vor v2 / nach Schritt 5 /
  // Cold-Boot) und für die V13-Mitigations-Entscheidung. `[budget]` ist
  // bewusst auf einer Zeile, damit `grep '\[budget\]'` aus dem Serial-Log
  // direkt die Tabelle ergibt.
  Serial.printf("[budget] ogd=%ums oebb=%ums total=%ums\n", summary.ogd_ms,
                summary.oebb_ms, summary.ogd_ms + summary.oebb_ms);
  Serial.printf("[heap] free_before_oebb=%u free_after_oebb=%u delta=%d\n",
                summary.free_heap_before_oebb, summary.free_heap_after_oebb,
                static_cast<int>(summary.free_heap_after_oebb) -
                    static_cast<int>(summary.free_heap_before_oebb));

  TEST_ASSERT_TRUE_MESSAGE(ok, "fetchSnapshot reported not ok");
  TEST_ASSERT_FALSE_MESSAGE(meta.auth_error_seen,
                            "auth tripwire fired on a happy-path fetch");

  for (int slot = 0; slot < SLOTS_PER_STREAM; ++slot) {
    char tag[24];
    std::snprintf(tag, sizeof(tag), "SBahn[%d]", slot);
    printSlot(tag, snap.stream[STREAM_SBAHN_HBF].slot[slot]);
  }

  TEST_ASSERT_TRUE_MESSAGE(snap.stream[STREAM_SBAHN_HBF].endpoint_responded,
                           "HAFAS mgate.exe did not respond (S-Bahn)");
  // Filter-matched depends on time-of-day (no trains overnight). Don't
  // hard-assert — but during daytime expect at least slot[0] to be valid and
  // carry a line_label. Soak gate (Session G) re-validates this at scale.
  if (snap.stream[STREAM_SBAHN_HBF].filter_matched) {
    TEST_ASSERT_TRUE_MESSAGE(snap.stream[STREAM_SBAHN_HBF].slot[0].valid,
                             "filter_matched but slot[0] is invalid");
    TEST_ASSERT_GREATER_THAN_MESSAGE(
        0, std::strlen(snap.stream[STREAM_SBAHN_HBF].slot[0].line_label),
        "line_label empty on a filter-matched S-Bahn slot");
  }
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
  printClock("boot (expected unsynced, < 1.7e9)");
  TEST_ASSERT_LESS_THAN_MESSAGE(MIN_PLAUSIBLE_EPOCH, g_clock.now(),
                                "clock unexpectedly synced at boot");
}

void test_ntp_sync_brings_clock_to_present(void) {
  Serial.printf("[ntp] calling ntpSync() against %s (TZ=%s)\n", NTP_SERVER,
                TZ_INFO);
  TEST_ASSERT_TRUE_MESSAGE(g_clock.ntpSync(), "ntpSync() failed");
  printClock("after ntpSync()");
  TEST_ASSERT_GREATER_THAN_MESSAGE(MIN_PLAUSIBLE_EPOCH, g_clock.now(),
                                   "clock still bogus after NTP");
}

void test_plansleep_with_unsynced_now_returns_huge(void) {
  // Reproduces the production symptom: when warmCyclePath passes now=0 to
  // planSleep (because the clock wasn't re-synced), the result is ~years.
  StreamSnapshot snap{};
  snap.api_ok = true;
  snap.stream[0].endpoint_responded = true;
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
  printClock("sim warm-boot pre-recovery");
  TEST_ASSERT_LESS_THAN_MESSAGE(MIN_PLAUSIBLE_EPOCH, g_clock.now(),
                                "precondition: clock must be bogus");

  // Production guard, literal copy from warmCyclePath:
  if (!g_clock.isSynced()) {
    TEST_ASSERT_TRUE_MESSAGE(g_clock.ntpSync(), "recovery NTP failed");
  }
  time_t after = g_clock.now();
  printClock("post-recovery");
  TEST_ASSERT_GREATER_THAN_MESSAGE(MIN_PLAUSIBLE_EPOCH, after,
                                   "recovery did not produce a valid clock");

  // planSleep with recovered clock against a bus 20 minutes out should be a
  // small positive deep-sleep interval — well under 24h, never years.
  StreamSnapshot snap{};
  snap.api_ok = true;
  snap.stream[0].endpoint_responded = true;
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
  printClock("plausibility check");
  time_t t = g_clock.now();
  struct tm local;
  localtime_r(&t, &local);
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
  if (!g_clock.isSynced()) {
    TEST_ASSERT_TRUE_MESSAGE(g_clock.ntpSync(), "guard NTP failed");
  }
  time_t clock_at_query_start = g_clock.now();
  Serial.printf("[engine] ordering: clock=%lld at query start\n",
                static_cast<long long>(clock_at_query_start));
  TEST_ASSERT_GREATER_THAN_MESSAGE(
      MIN_PLAUSIBLE_EPOCH, clock_at_query_start,
      "ORDERING VIOLATION: clock bogus when query begins");

  std::string body;
  {
    auto _r = g_net.httpGet(ogdProbeUrl(), body);
    TEST_ASSERT_TRUE_MESSAGE(_r.ok && _r.http_status >= 200 &&
                                 _r.http_status < 300,
                             "ordering test: httpGet non-2xx");
  }

  time_t clock_after_query = g_clock.now();
  Serial.printf(
      "[engine] ordering: clock=%lld after query (Δ=%lld s)\n",
      static_cast<long long>(clock_after_query),
      static_cast<long long>(clock_after_query - clock_at_query_start));
  TEST_ASSERT_TRUE_MESSAGE(clock_after_query - clock_at_query_start < 30,
                           "clock jumped >30 s during query — NTP fired "
                           "again mid-query?");
}

void test_plansleep_with_synced_now_is_sane(void) {
  // With a synced clock and a bus 60s in the future, planSleep should pick
  // Active mode (delta < active_threshold).
  StreamSnapshot snap{};
  snap.api_ok = true;
  snap.stream[0].endpoint_responded = true;
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
  RUN_TEST(test_parse_all_bus_streams_respond);
  RUN_TEST(test_fetchSnapshot_fills_oebb_stream);

  // Clock is now synced (by the recovery test). These verify post-sync state.
  RUN_TEST(test_ntp_sync_brings_clock_to_present);
  RUN_TEST(test_clock_reads_plausible_current_time);
  RUN_TEST(test_ntp_completes_before_api_query);
  RUN_TEST(test_plansleep_with_synced_now_is_sane);
  UNITY_END();
}

void loop() {}
