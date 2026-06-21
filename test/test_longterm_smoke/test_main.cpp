// Long-term HW-Sanity smoke test (~3 min). Walks the full production
// pipeline once on real hardware after a flash: WiFi-Up -> NTP -> HTTPS
// GET vs. live Wiener-Linien API -> parse -> filter -> heap-check. No
// retry-budgets, no display assertions — just "läuft die Kiste
// überhaupt noch" after a hardware intervention. See docs/TESTING.md
// §13.3 "Hardware-Eingriff" for when to run.
//
// Run via `make test-longterm-smoke` (env:longterm-smoke).

#include "../test_longterm_soak/RecordingDisplay.h"
#include "config.h"
#include "data/wienerlinien_parse.h"
#include "hal/Esp32Clock.h"
#include "hal/Esp32Network.h"
#include "hal/IPersistentStore.h"
#include "logic/api_fetcher.h"
#include "logic/display_apply.h"
#include "logic/refresh_planner.h"
#include "logic/slot_merger.h"
#include "render/layout.h"
#include "secrets.h"

#include <Arduino.h>
#include <cstdio>
#include <cstring>
#include <unity.h>

using namespace bustaferl;

namespace {

constexpr uint32_t MIN_FREE_HEAP_BYTES = 60 * 1024; // alarm threshold
constexpr uint32_t MIN_BODY_BYTES = 200;            // any sane response
constexpr uint32_t SETTLE_DELAY_S = 30;             // long enough that a
                                                    // marginal panel / WiFi
                                                    // edge case has time to
                                                    // surface
Esp32Network g_net;
Esp32Clock g_clock{NTP_SERVER, TZ_INFO};
bustaferl::test::RecordingDisplay g_display;
Frame g_frame_new;
Frame g_frame_prev;
PersistedMeta g_disp_meta;
uint32_t g_initial_heap = 0;

std::string apiUrl() {
  std::string url = WL_API_BASE;
  char buf[96];
  std::snprintf(buf, sizeof(buf), "&stopId=%d&stopId=%d&stopId=%d",
                RBL_TULL_ATZGERSDORF, RBL_TULL_HIETZING, RBL_ENDEMANN);
  url += buf;
  return url;
}

void buildFilters(StreamFilter (&f)[STREAM_COUNT]) {
  f[STREAM_58A_ATZ] = {RBL_TULL_ATZGERSDORF, LINE_58A, TOWARDS_58A_ATZ};
  f[STREAM_58A_HIETZING] = {RBL_TULL_HIETZING, LINE_58A, TOWARDS_58A_HIETZING};
  f[STREAM_58B_ATZ] = {RBL_ENDEMANN, LINE_58B, FILTER_TOWARDS_58B};
  // STREAM_SBAHN_HBF left default (rbl = 0) — not fetched via OGD here.
}

} // namespace

void test_initial_heap_above_threshold(void) {
  g_initial_heap = ESP.getFreeHeap();
  Serial.printf("[smoke] initial free_heap=%u\n", g_initial_heap);
  TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
      MIN_FREE_HEAP_BYTES, g_initial_heap,
      "free heap below smoke-test threshold at cold boot");
}

void test_wifi_connects(void) {
  g_net.addAp(WIFI_SSID_PRIMARY, WIFI_PASSWORD_PRIMARY);
#ifdef WIFI_SSID_SECONDARY
  g_net.addAp(WIFI_SSID_SECONDARY, WIFI_PASSWORD_SECONDARY);
#endif
  TEST_ASSERT_TRUE_MESSAGE(g_net.connect(15000),
                           "smoke: WiFi did not connect in 15 s");
}

void test_ntp_syncs(void) {
  TEST_ASSERT_TRUE_MESSAGE(g_clock.ntpSync(), "smoke: initial NTP sync failed");
  time_t t = g_clock.now();
  Serial.printf("[smoke] clock_epoch=%lld\n", static_cast<long long>(t));
  TEST_ASSERT_GREATER_THAN_INT_MESSAGE(
      MIN_PLAUSIBLE_EPOCH, static_cast<int>(t),
      "smoke: clock is bogus after NTP sync (real time should be >= 2023)");
}

void test_pipeline_one_full_cycle(void) {
  std::string body;
  FetchConfig fc;
  FetchOutcome fo = fetchWithRetry(g_net, apiUrl(), body, fc);
  Serial.printf("[smoke] fetch ok=%d attempts=%d body=%u\n", fo.ok,
                fo.attempts_taken, static_cast<unsigned>(body.size()));
  TEST_ASSERT_TRUE_MESSAGE(fo.ok, "smoke: live HTTPS fetch failed");
  TEST_ASSERT_GREATER_THAN_UINT_MESSAGE(MIN_BODY_BYTES, body.size(),
                                        "smoke: live body suspiciously small");

  StreamFilter filters[STREAM_COUNT];
  buildFilters(filters);
  StreamSnapshot snap;
  bool parsed = parseMonitorResponse(body, filters, snap);
  TEST_ASSERT_TRUE_MESSAGE(parsed, "smoke: parse of live body failed");

  int endpoints_responded = 0;
  for (int i = 0; i < STREAM_COUNT; ++i)
    endpoints_responded += snap.stream[i].endpoint_responded ? 1 : 0;
  Serial.printf("[smoke] endpoints_responded=%d/%d\n", endpoints_responded,
                STREAM_COUNT);
  TEST_ASSERT_GREATER_OR_EQUAL_INT_MESSAGE(
      3, endpoints_responded,
      "smoke: fewer than 3 endpoints responded — hardware/network suspect");

  // Walk the render + planRefresh + RecordingDisplay (RLE roundtrip)
  // path once on the live parse result. Catches "renderFrame blows up
  // on the live JSON shape" or "RLE overflows on live data" before the
  // longer-running tests do (Schritt 0a.2).
  ScheduleSnapshot empty_schedule;
  time_t now_t = g_clock.now();
  StreamSnapshot merged_for_render = mergeSlots(snap, empty_schedule, now_t);
  RenderInput in{merged_for_render, OverlayKind::None};
  renderFrame(in, g_frame_new);
  RefreshConfig rc;
  RefreshDecision rd = planRefresh(
      g_frame_prev.data(), g_frame_new.data(), /*prev_valid=*/false, now_t,
      g_disp_meta.last_light_full, g_disp_meta.partial_count, rc);
  applyDisplayDecision(g_display, rd, g_frame_new.data(), g_disp_meta, now_t);
  Serial.printf("[smoke] display: full=%d partial=%d light=%d deep=%d "
                "rle_overflow=%d\n",
                g_display.full_calls, g_display.partial_calls,
                g_display.light_full_calls, g_display.deep_clean_calls,
                g_display.rle_overflow_observed);
  TEST_ASSERT_EQUAL_INT_MESSAGE(
      0, g_display.rle_overflow_observed,
      "smoke: live framebuffer overflowed the prod-cap RLE budget");
}

void test_settle_window_then_heap_stable(void) {
  Serial.printf("[smoke] settling for %u s ...\n", SETTLE_DELAY_S);
  for (uint32_t i = 0; i < SETTLE_DELAY_S; ++i) {
    delay(1000);
  }
  uint32_t now = ESP.getFreeHeap();
  int drift = static_cast<int>(g_initial_heap) - static_cast<int>(now);
  Serial.printf("[smoke] heap initial=%u now=%u drift=%d\n", g_initial_heap,
                now, drift);
  TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
      MIN_FREE_HEAP_BYTES, now,
      "smoke: free heap below threshold after one full cycle + settle");
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  UNITY_BEGIN();
  RUN_TEST(test_initial_heap_above_threshold);
  RUN_TEST(test_wifi_connects);
  RUN_TEST(test_ntp_syncs);
  RUN_TEST(test_pipeline_one_full_cycle);
  RUN_TEST(test_settle_window_then_heap_stable);
  UNITY_END();
}

void loop() {}
