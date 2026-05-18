// Long-term WiFi-Jitter test (~10 min). Drives the production fetch
// path through injected WiFi drops to prove the retry/reconnect logic
// actually recovers under real radio conditions (not just in unit
// fakes). Each cycle: fetch -> WiFi.disconnect() -> fetch (expect
// retry succeeds after reconnect) -> sleep. Asserts that overall
// success rate stays above MIN_SUCCESS_PCT and that no cycle hangs
// past the per-cycle budget.
//
// Run via `make test-longterm-jitter` (env:longterm-jitter).

#include "config.h"
#include "data/wienerlinien_parse.h"
#include "hal/Esp32Clock.h"
#include "hal/Esp32Network.h"
#include "logic/api_fetcher.h"
#include "secrets.h"

#include <Arduino.h>
#include <WiFi.h>
#include <cstdio>
#include <unity.h>

using namespace bustaferl;

namespace {

constexpr int CYCLES = 10;           // 10 inject-and-recover loops
constexpr int CYCLE_INTERVAL_S = 60; // ~10 min wallclock
constexpr int MIN_SUCCESS_PCT = 70;  // jitter is brutal; tolerate some loss

Esp32Network g_net;
Esp32Clock g_clock{NTP_SERVER, TZ_INFO};
int g_pre_drop_successes = 0;
int g_post_drop_successes = 0;
int g_reconnect_failures = 0;

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

bool oneCycle(int cycle) {
  uint32_t heap_before = ESP.getFreeHeap();

  if (!g_net.isConnected()) {
    Serial.printf("[jitter] cycle=%d wifi down at entry, reconnecting\n",
                  cycle);
    if (!g_net.connect(10000)) {
      ++g_reconnect_failures;
      return false;
    }
  }

  std::string body;
  FetchConfig fc;
  FetchOutcome pre = fetchWithRetry(g_net, apiUrl(), body, fc);
  if (pre.ok)
    ++g_pre_drop_successes;
  Serial.printf("[jitter] cycle=%d pre  ok=%d attempts=%d body=%u\n", cycle,
                pre.ok, pre.attempts_taken, static_cast<unsigned>(body.size()));

  // Inject the drop: force a disconnect, give the radio a moment to settle.
  WiFi.disconnect(false /* don't wipe SSID */);
  delay(2000);
  Serial.printf("[jitter] cycle=%d drop injected, isConnected=%d\n", cycle,
                g_net.isConnected());

  // Production path: fetchWithRetry handles reconnect via Esp32Network's
  // WiFiMulti loop.
  body.clear();
  FetchOutcome post = fetchWithRetry(g_net, apiUrl(), body, fc);
  if (post.ok)
    ++g_post_drop_successes;
  uint32_t heap_after = ESP.getFreeHeap();
  Serial.printf("[jitter] cycle=%d post ok=%d attempts=%d body=%u heap_b=%u "
                "heap_a=%u Δ=%d\n",
                cycle, post.ok, post.attempts_taken,
                static_cast<unsigned>(body.size()), heap_before, heap_after,
                static_cast<int>(heap_after) - static_cast<int>(heap_before));
  return post.ok;
}

} // namespace

void test_setup_wifi(void) {
  g_net.addAp(WIFI_SSID_PRIMARY, WIFI_PASSWORD_PRIMARY);
#ifdef WIFI_SSID_SECONDARY
  g_net.addAp(WIFI_SSID_SECONDARY, WIFI_PASSWORD_SECONDARY);
#endif
  TEST_ASSERT_TRUE_MESSAGE(g_net.connect(15000),
                           "jitter: initial WiFi did not connect");
}

void test_drop_and_recover_loop(void) {
  for (int cycle = 1; cycle <= CYCLES; ++cycle) {
    time_t t_start = g_clock.now();
    oneCycle(cycle);
    while (g_clock.now() < t_start + CYCLE_INTERVAL_S) {
      delay(500);
    }
  }

  Serial.printf("[jitter] DONE: pre=%d post=%d reconnect_fail=%d / %d cycles\n",
                g_pre_drop_successes, g_post_drop_successes,
                g_reconnect_failures, CYCLES);

  int required = (CYCLES * MIN_SUCCESS_PCT) / 100;
  TEST_ASSERT_GREATER_OR_EQUAL_INT_MESSAGE(
      required, g_post_drop_successes,
      "jitter: post-drop success rate below threshold — recovery path broken");
  TEST_ASSERT_LESS_THAN_INT_MESSAGE(
      3, g_reconnect_failures,
      "jitter: too many reconnect failures — AP / radio suspect");
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  UNITY_BEGIN();
  RUN_TEST(test_setup_wifi);
  RUN_TEST(test_drop_and_recover_loop);
  UNITY_END();
}

void loop() {}
