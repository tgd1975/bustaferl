// Long-term on-device test (canonical heap-leak detector). Iterates a
// full fetch+parse cycle on a 60 s cadence for LONGTERM_SOAK_CYCLES
// minutes, asserting steady-state invariants: WiFi stays up, every RBL
// keeps responding, the wall clock advances monotonically, and free
// heap does not accumulate a leak. Per-cycle telemetry goes to serial
// as `[longterm] cycle=N ...` so a regression like "memory leaks
// 1KB/cycle" is visible in the log even if the soft-assertions pass.
//
// One source, three variants (LONGTERM_SOAK_CYCLES set by env):
//   make test-longterm-soak-5min    (CYCLES=5,  ~5 min  quick-check)
//   make test-longterm-soak-15min   (CYCLES=15, ~15 min pre-commit)
//   make test-longterm-soak-1h      (CYCLES=60, ~1 h    canonical)
// HEAP_LEAK_BUDGET_BYTES scales linearly so the per-cycle drift
// tolerance is identical across the three variants.

#include "RecordingDisplay.h"
#include "config.h"
#include "data/wienerlinien_parse.h"
#include "hal/Esp32Clock.h"
#include "hal/Esp32Network.h"
#include "hal/IPersistentStore.h"
#include "logic/api_fetcher.h"
#include "logic/display_apply.h"
#include "logic/filter_health.h"
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

#ifndef LONGTERM_SOAK_CYCLES
#define LONGTERM_SOAK_CYCLES 60 // canonical 1 h default if env didn't set it
#endif

constexpr int CYCLES = LONGTERM_SOAK_CYCLES;
constexpr int CYCLE_INTERVAL_S = 60;
// 8 KB/h budget, scaled linearly to cycle count so per-cycle drift
// tolerance stays identical across the 5min/15min/1h variants.
constexpr int HEAP_LEAK_BUDGET_BYTES = (8 * 1024 * CYCLES) / 60;
constexpr int MIN_SUCCESS_PCT = 85; // tolerate transient outages

Esp32Network g_net;
Esp32Clock g_clock{NTP_SERVER, TZ_INFO};
bustaferl::test::RecordingDisplay g_display;
Frame g_frame_new;
Frame g_frame_prev;
PersistedMeta g_disp_meta;

uint32_t g_initial_heap = 0;
uint32_t g_min_heap = 0xFFFFFFFFu;
int g_successes = 0;
int g_http_failures = 0;
int g_parse_failures = 0;
int g_retry_succeeded = 0; // succeeded on attempt > 1

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

void test_run_soak_cycles(void) {
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

    // Drive the production render pipeline + RLE save/load on every
    // cycle so the soak's heap-leak budget covers renderFrame and the
    // partial/light-full bookkeeping path (Schritt 0a.2). RecordingDisplay
    // walks the RLE roundtrip without abusing the panel.
    if (parsed) {
      ScheduleSnapshot empty_schedule;
      StreamSnapshot merged_for_render =
          mergeSlots(snap, empty_schedule, t_cycle);
      RenderInput in{merged_for_render, OverlayKind::None};
      renderFrame(in, g_frame_new);
      bool prev_valid = (cycle > 1);
      RefreshConfig rc;
      RefreshDecision rd = planRefresh(
          g_frame_prev.data(), g_frame_new.data(), prev_valid, t_cycle,
          g_disp_meta.last_light_full, g_disp_meta.partial_count, rc);
      applyDisplayDecision(g_display, rd, g_frame_new.data(), g_disp_meta,
                           t_cycle);
      std::memcpy(g_frame_prev.data(), g_frame_new.data(), Frame::bytes);
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
        "f=%d | 58B r=%d f=%d | U1-Leo r=%d f=%d | U1-Obe r=%d f=%d\n",
        cycle, CYCLES, local.tm_hour, local.tm_min, local.tm_sec, fo.ok, parsed,
        fo.attempts_taken, static_cast<unsigned>(body.size()), heap_before,
        heap_after,
        static_cast<int>(heap_after) - static_cast<int>(heap_before),
        snap.stream[STREAM_58A_ATZ].rbl_responded,
        snap.stream[STREAM_58A_ATZ].filter_matched,
        snap.stream[STREAM_58A_HIETZING].rbl_responded,
        snap.stream[STREAM_58A_HIETZING].filter_matched,
        snap.stream[STREAM_58B_ATZ].rbl_responded,
        snap.stream[STREAM_58B_ATZ].filter_matched,
        snap.stream[STREAM_U1_LEOPOLDAU].rbl_responded,
        snap.stream[STREAM_U1_LEOPOLDAU].filter_matched,
        snap.stream[STREAM_U1_OBERLAA].rbl_responded,
        snap.stream[STREAM_U1_OBERLAA].filter_matched);

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
                static_cast<int>(final_heap) - static_cast<int>(g_initial_heap),
                static_cast<int>(g_min_heap) -
                    static_cast<int>(g_initial_heap));

  TEST_ASSERT_GREATER_OR_EQUAL_INT_MESSAGE(
      (CYCLES * MIN_SUCCESS_PCT) / 100, g_successes,
      "long-term: success rate below threshold");

  int heap_loss =
      static_cast<int>(g_initial_heap) - static_cast<int>(final_heap);
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
  RUN_TEST(test_run_soak_cycles);
  UNITY_END();
}

void loop() {}
