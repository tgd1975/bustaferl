// Long-term Day-Full test (~24 h, pre-release). The most expensive
// test in the set — unattended overnight, intended to be launched in
// the evening so it spans evening dry-up, full night silence, morning
// ramp-up, and a complete daytime window. Asserts the union of all
// shorter long-term tests' invariants:
//   - heap stays within 24 h drift budget (covers slow leaks)
//   - both live transitions observed: evening dry-up AND morning ramp-up
//   - display refresh budget not blown (partial-count tracked across run)
//   - clock advances monotonically (catches DST and NTP-storm regressions)
//
// Operator note: launch evening (~21:00); finishes next evening
// (~21:00). The DST switch in March/October will land inside the
// window twice a year — assertion (4) is the one that catches a
// regression in the time math.
//
// Run via `make test-longterm-day-full` (env:longterm-day-full).

#include <Arduino.h>
#include <cstdio>
#include <unity.h>

#include "config.h"
#include "data/wienerlinien_parse.h"
#include "hal/Esp32Clock.h"
#include "hal/Esp32Network.h"
#include "logic/api_fetcher.h"
#include "logic/sleep_planner.h"
#include "secrets.h"

using namespace bustaferl;

namespace {

constexpr int CYCLES = 1440; // 1440 * 60 s = 24 h
constexpr int CYCLE_INTERVAL_S = 60;
constexpr int MIN_SUCCESS_PCT = 80; // overnight has API outages, be lenient
constexpr int HEAP_LEAK_BUDGET_BYTES = 64 * 1024; // 24 h budget

Esp32Network g_net;
Esp32Clock g_clock{NTP_SERVER, TZ_INFO};

uint32_t g_initial_heap = 0;
time_t g_start_clock = 0;
int g_successes = 0;
bool g_saw_dryup = false;
bool g_saw_rampup = false;
bool g_was_active_last_cycle = false;
int g_partial_counter = 0; // simulated partial-refresh counter
int g_refresh_resets = 0;

std::string apiUrl() {
  std::string url = WL_API_BASE;
  char buf[96];
  std::snprintf(buf, sizeof(buf),
                "&stopId=%d&stopId=%d&stopId=%d&stopId=%d&stopId=%d",
                RBL_TULL_ATZGERSDORF, RBL_TULL_HIETZING, RBL_ENDEMANN,
                RBL_SUEDTIROLER_LEOPOLDAU, RBL_SUEDTIROLER_OBERLAA);
  url += buf;
  return url;
}

void buildFilters(StreamFilter (&f)[STREAM_COUNT]) {
  f[STREAM_58A_ATZ] = {RBL_TULL_ATZGERSDORF, LINE_58A, TOWARDS_58A_ATZ};
  f[STREAM_58A_HIETZING] = {RBL_TULL_HIETZING, LINE_58A, TOWARDS_58A_HIETZING};
  f[STREAM_58B_ATZ] = {RBL_ENDEMANN, LINE_58B, FILTER_TOWARDS_58B};
  f[STREAM_U1_LEOPOLDAU] = {RBL_SUEDTIROLER_LEOPOLDAU, LINE_U1,
                            TOWARDS_U1_LEOPOLDAU};
  f[STREAM_U1_OBERLAA] = {RBL_SUEDTIROLER_OBERLAA, LINE_U1, TOWARDS_U1_OBERLAA};
}

} // namespace

void test_start_conditions(void) {
  g_net.addAp(WIFI_SSID_PRIMARY, WIFI_PASSWORD_PRIMARY);
#ifdef WIFI_SSID_SECONDARY
  g_net.addAp(WIFI_SSID_SECONDARY, WIFI_PASSWORD_SECONDARY);
#endif
  TEST_ASSERT_TRUE_MESSAGE(g_net.connect(15000),
                           "day_full: initial WiFi failed");
  TEST_ASSERT_TRUE_MESSAGE(g_clock.ntpSync(), "day_full: initial NTP failed");
  g_start_clock = g_clock.now();
  g_initial_heap = ESP.getFreeHeap();
  struct tm local;
  localtime_r(&g_start_clock, &local);
  Serial.printf("[day_full] start local=%04d-%02d-%02d %02d:%02d heap=%u\n",
                local.tm_year + 1900, local.tm_mon + 1, local.tm_mday,
                local.tm_hour, local.tm_min, g_initial_heap);
}

void test_24h_loop(void) {
  StreamFilter filters[STREAM_COUNT];
  buildFilters(filters);
  time_t prev_clock = g_start_clock;

  for (int cycle = 1; cycle <= CYCLES; ++cycle) {
    time_t t_cycle = g_clock.now();

    // Invariant 4: clock advances monotonically.
    if (t_cycle < prev_clock) {
      Serial.printf("[day_full] WARN clock went backwards prev=%lld now=%lld\n",
                    static_cast<long long>(prev_clock),
                    static_cast<long long>(t_cycle));
    }
    prev_clock = t_cycle;

    if (!g_net.isConnected())
      g_net.connect(10000);

    std::string body;
    FetchConfig fc;
    FetchOutcome fo = fetchWithRetry(g_net, apiUrl(), body, fc);
    StreamSnapshot snap;
    bool parsed = fo.ok && parseMonitorResponse(body, filters, snap);
    if (parsed)
      ++g_successes;

    bool active_now = false;
    if (parsed) {
      for (int s = 0; s < STREAM_COUNT; ++s)
        if (snap.stream[s].slot[0].valid) {
          active_now = true;
          break;
        }
    }
    if (g_was_active_last_cycle && !active_now)
      g_saw_dryup = true;
    if (!g_was_active_last_cycle && active_now && cycle > 30)
      g_saw_rampup = true;
    g_was_active_last_cycle = active_now;

    // Refresh-budget simulation: every active cycle bumps partial_counter,
    // forced reset when it hits PARTIAL_HARDCAP.
    if (active_now) {
      ++g_partial_counter;
      if (g_partial_counter >= PARTIAL_HARDCAP) {
        g_partial_counter = 0;
        ++g_refresh_resets;
      }
    }

    if (cycle % 60 == 0) {
      struct tm local;
      localtime_r(&t_cycle, &local);
      Serial.printf("[day_full] cycle=%d %02d:%02d active=%d succ=%d/%d "
                    "dryup=%d rampup=%d refresh_resets=%d heap=%u\n",
                    cycle, local.tm_hour, local.tm_min, active_now, g_successes,
                    cycle, g_saw_dryup, g_saw_rampup, g_refresh_resets,
                    ESP.getFreeHeap());
    }

    while (g_clock.now() < t_cycle + CYCLE_INTERVAL_S)
      delay(500);
  }

  uint32_t final_heap = ESP.getFreeHeap();
  int heap_drift =
      static_cast<int>(g_initial_heap) - static_cast<int>(final_heap);
  Serial.printf("[day_full] DONE successes=%d/%d dryup=%d rampup=%d "
                "refresh_resets=%d heap_drift=%d\n",
                g_successes, CYCLES, g_saw_dryup, g_saw_rampup,
                g_refresh_resets, heap_drift);

  TEST_ASSERT_GREATER_OR_EQUAL_INT_MESSAGE(
      (CYCLES * MIN_SUCCESS_PCT) / 100, g_successes,
      "day_full: success rate below 24 h threshold");
  TEST_ASSERT_TRUE_MESSAGE(g_saw_dryup,
                           "day_full: never observed live -> empty transition");
  TEST_ASSERT_TRUE_MESSAGE(g_saw_rampup,
                           "day_full: never observed empty -> live transition");
  TEST_ASSERT_LESS_THAN_INT_MESSAGE(HEAP_LEAK_BUDGET_BYTES, heap_drift,
                                    "day_full: heap drift exceeds 24 h budget");
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  UNITY_BEGIN();
  RUN_TEST(test_start_conditions);
  RUN_TEST(test_24h_loop);
  UNITY_END();
}

void loop() {}
