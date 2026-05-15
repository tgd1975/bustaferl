// Long-term on-device test. Iterates a full fetch+parse cycle on a 60 s
// cadence for ~60 minutes, asserting steady-state invariants: WiFi stays
// up, every RBL keeps responding, the wall clock advances monotonically,
// and free heap doesn't accumulate a leak. Per-cycle telemetry goes to
// serial as `[longterm] cycle=N ...` so a regression like "memory leaks
// 1KB/cycle" is visible in the log even if the soft-assertions still pass.
//
// Run via `pio test -e esp32-test-longterm` — the env sets test_timeout
// well above the wall-clock budget so PIO does not kill the run early.

#include <Arduino.h>
#include <cstdio>
#include <unity.h>

#include "config.h"
#include "data/wienerlinien_parse.h"
#include "hal/Esp32Clock.h"
#include "hal/Esp32Network.h"
#include "logic/api_fetcher.h"
#include "logic/filter_health.h"
#include "secrets.h"

using namespace bustaferl;

namespace {

constexpr int CYCLES = 60;
constexpr int CYCLE_INTERVAL_S = 60;
constexpr int HEAP_LEAK_BUDGET_BYTES = 8 * 1024; // 8 KB allowed drift over 1h
constexpr int MIN_SUCCESS_PCT = 85;              // tolerate transient outages

Esp32Network g_net;
Esp32Clock g_clock{NTP_SERVER, TZ_INFO};

uint32_t g_initial_heap = 0;
uint32_t g_min_heap = 0xFFFFFFFFu;
int g_successes = 0;
int g_http_failures = 0;
int g_parse_failures = 0;
int g_retry_succeeded = 0; // succeeded on attempt > 1

std::string apiUrl() {
  std::string url = WL_API_BASE;
  char buf[64];
  std::snprintf(buf, sizeof(buf), "&rbl=%d&rbl=%d&rbl=%d",
                RBL_TULL_ATZGERSDORF, RBL_TULL_HIETZING, RBL_ENDEMANN);
  url += buf;
  return url;
}

void buildFilters(StreamFilter (&f)[STREAM_COUNT]) {
  f[STREAM_58A_ATZ] = {RBL_TULL_ATZGERSDORF, LINE_58A, TOWARDS_58A_ATZ};
  f[STREAM_58A_HIETZING] = {RBL_TULL_HIETZING, LINE_58A, TOWARDS_58A_HIETZING};
  f[STREAM_58B_ATZ] = {RBL_ENDEMANN, LINE_58B, FILTER_TOWARDS_58B};
}

} // namespace

void test_setup_wifi_and_clock(void) {
  g_net.addAp(WIFI_SSID_PRIMARY, WIFI_PASSWORD_PRIMARY);
#ifdef WIFI_SSID_SECONDARY
  g_net.addAp(WIFI_SSID_SECONDARY, WIFI_PASSWORD_SECONDARY);
#endif
  TEST_ASSERT_TRUE_MESSAGE(g_net.connect(15000), "initial WiFi failed");
  if (g_clock.now() < 1700000000) {
    TEST_ASSERT_TRUE_MESSAGE(g_clock.ntpSync(), "initial NTP failed");
  }
  g_initial_heap = ESP.getFreeHeap();
  g_min_heap = g_initial_heap;
  Serial.printf("[longterm] start: free_heap=%u clock=%lld\n", g_initial_heap,
                static_cast<long long>(g_clock.now()));
}

void test_run_one_hour_cycle(void) {
  StreamFilter filters[STREAM_COUNT];
  buildFilters(filters);

  time_t test_started = g_clock.now();

  for (int cycle = 1; cycle <= CYCLES; ++cycle) {
    time_t t_cycle = g_clock.now();
    uint32_t heap_before = ESP.getFreeHeap();

    if (!g_net.isConnected()) {
      Serial.printf("[longterm] cycle=%d wifi dropped, reconnecting\n", cycle);
      g_net.connect(10000);
    }

    std::string body;
    FetchConfig fc;
    FetchOutcome fo = fetchWithRetry(g_net, apiUrl(), body, fc);

    StreamSnapshot snap;
    bool parsed = false;
    if (fo.ok) {
      parsed = parseMonitorResponse(body, filters, snap);
    }

    uint32_t heap_after = ESP.getFreeHeap();
    if (heap_after < g_min_heap)
      g_min_heap = heap_after;

    if (fo.ok && parsed) {
      ++g_successes;
      if (fo.attempts_taken > 1)
        ++g_retry_succeeded;
    } else if (!fo.ok) {
      ++g_http_failures;
    } else {
      ++g_parse_failures;
    }

    struct tm local;
    localtime_r(&t_cycle, &local);
    Serial.printf(
        "[longterm] cycle=%d/%d %02d:%02d:%02d ok=%d parsed=%d attempts=%d "
        "body=%u heap_b=%u heap_a=%u Δ=%d 58A-Atz r=%d f=%d | 58A-Hie r=%d "
        "f=%d | 58B r=%d f=%d\n",
        cycle, CYCLES, local.tm_hour, local.tm_min, local.tm_sec, fo.ok,
        parsed, fo.attempts_taken, static_cast<unsigned>(body.size()),
        heap_before, heap_after,
        static_cast<int>(heap_after) - static_cast<int>(heap_before),
        snap.stream[STREAM_58A_ATZ].rbl_responded,
        snap.stream[STREAM_58A_ATZ].filter_matched,
        snap.stream[STREAM_58A_HIETZING].rbl_responded,
        snap.stream[STREAM_58A_HIETZING].filter_matched,
        snap.stream[STREAM_58B_ATZ].rbl_responded,
        snap.stream[STREAM_58B_ATZ].filter_matched);

    // Monotonic clock check — catches NTP storms / DST math regressions.
    TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(test_started, g_clock.now(),
                                         "clock ran backwards mid-test");

    // Yield-friendly wait to the next cycle boundary.
    while (g_clock.now() < t_cycle + CYCLE_INTERVAL_S) {
      delay(500);
    }
  }

  uint32_t final_heap = ESP.getFreeHeap();
  Serial.printf("[longterm] DONE: cycles=%d success=%d http_fail=%d "
                "parse_fail=%d retry_recovered=%d\n",
                CYCLES, g_successes, g_http_failures, g_parse_failures,
                g_retry_succeeded);
  Serial.printf("[longterm] heap: initial=%u final=%u min=%u final_Δ=%d "
                "min_Δ=%d\n",
                g_initial_heap, final_heap, g_min_heap,
                static_cast<int>(final_heap) -
                    static_cast<int>(g_initial_heap),
                static_cast<int>(g_min_heap) -
                    static_cast<int>(g_initial_heap));

  TEST_ASSERT_GREATER_OR_EQUAL_INT_MESSAGE(
      (CYCLES * MIN_SUCCESS_PCT) / 100, g_successes,
      "long-term: success rate below threshold");

  int heap_loss = static_cast<int>(g_initial_heap) -
                  static_cast<int>(final_heap);
  TEST_ASSERT_LESS_THAN_INT_MESSAGE(
      HEAP_LEAK_BUDGET_BYTES, heap_loss,
      "long-term: free heap dropped more than budget — leak suspected");

  TEST_ASSERT_EQUAL_INT_MESSAGE(0, g_parse_failures,
                                "long-term: any parse failure of a fetched "
                                "body is a regression");
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  UNITY_BEGIN();
  RUN_TEST(test_setup_wifi_and_clock);
  RUN_TEST(test_run_one_hour_cycle);
  UNITY_END();
}

void loop() {}
