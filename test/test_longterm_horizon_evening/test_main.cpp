// Long-term Horizon-Evening test (~5 h). Starts at ~21:30 local time
// and runs through the evening dry-up: U1 has earlier night-stop than
// the 58A/B buses, so the streams empty out at different cycles. The
// test asserts (a) at least one stream actively dries up during the
// observation window, (b) the sleep-planner returns NO_DATA_SLEEP_S
// at least once after all live streams empty, and (c) heap stays
// stable through the transition into nighttime silence.
//
// Operator note: launch ~21:30; finishes ~02:30. PIO's default test
// timeout may not tolerate 5 h — if it kills the run, drive this via
// `make test-device-trace ENV=longterm-horizon-evening` so the partial
// log lands in `.tmp/traces/` even on timeout.
//
// Run via `make test-longterm-horizon-evening`.

#include "config.h"
#include "data/wienerlinien_parse.h"
#include "hal/Esp32Clock.h"
#include "hal/Esp32Display.h"
#include "hal/Esp32Network.h"
#include "hal/IPersistentStore.h"
#include "logic/api_fetcher.h"
#include "logic/display_apply.h"
#include "logic/refresh_planner.h"
#include "logic/sleep_planner.h"
#include "logic/slot_merger.h"
#include "render/layout.h"
#include "secrets.h"

#include <Arduino.h>
#include <cstdio>
#include <cstring>
#include <unity.h>

using namespace bustaferl;

namespace {

constexpr int CYCLES = 300; // 300 * 60 s = 5 h
constexpr int CYCLE_INTERVAL_S = 60;
constexpr int HEAP_LEAK_BUDGET_BYTES = 40 * 1024; // 5 h budget

Esp32Network g_net;
Esp32Clock g_clock{NTP_SERVER, TZ_INFO};
Esp32Display g_display;
// 15 KB framebuffers must live in BSS, not on the stack — see
// claude-code memory `frame-must-be-heap`.
Frame g_frame_new;
Frame g_frame_prev;
PersistedMeta g_disp_meta; // local-only: drives partial/light-full bookkeeping

uint32_t g_initial_heap = 0;
int g_successes = 0;
int g_no_data_sleep_decisions = 0;
int g_partial_count = 0;
int g_light_full_count = 0;
int g_deep_clean_count = 0;
int g_stream_dryup_observed[STREAM_COUNT] = {0}; // counts cycles a stream
                                                 // went from non-empty
                                                 // to empty
bool g_stream_was_active[STREAM_COUNT] = {false};

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

void test_evening_start_conditions(void) {
  g_net.addAp(WIFI_SSID_PRIMARY, WIFI_PASSWORD_PRIMARY);
#ifdef WIFI_SSID_SECONDARY
  g_net.addAp(WIFI_SSID_SECONDARY, WIFI_PASSWORD_SECONDARY);
#endif
  TEST_ASSERT_TRUE_MESSAGE(g_net.connect(15000),
                           "evening: initial WiFi failed");
  TEST_ASSERT_TRUE_MESSAGE(g_clock.ntpSync(), "evening: initial NTP failed");
  time_t now = g_clock.now();
  struct tm local;
  localtime_r(&now, &local);
  Serial.printf("[evening] starting at local=%04d-%02d-%02d %02d:%02d:%02d\n",
                local.tm_year + 1900, local.tm_mon + 1, local.tm_mday,
                local.tm_hour, local.tm_min, local.tm_sec);
  TEST_ASSERT_TRUE_MESSAGE(
      local.tm_hour >= 20 || local.tm_hour <= 3,
      "evening: launched outside the 20:00–03:00 window the test expects");
  // Bring the e-paper panel up so per-cycle refresh has a target. Heap
  // for the GxEPD2 panel is reserved here, before g_initial_heap is
  // sampled, so it doesn't show up as drift later.
  g_display.init();
  g_initial_heap = ESP.getFreeHeap();
}

void test_dryup_and_sleep_decisions(void) {
  StreamFilter filters[STREAM_COUNT];
  buildFilters(filters);

  for (int cycle = 1; cycle <= CYCLES; ++cycle) {
    time_t t_cycle = g_clock.now();
    if (!g_net.isConnected())
      g_net.connect(10000);

    std::string body;
    FetchConfig fc;
    FetchOutcome fo = fetchWithRetry(g_net, apiUrl(), body, fc);
    StreamSnapshot snap;
    bool parsed = fo.ok && parseMonitorResponse(body, filters, snap);
    if (parsed)
      ++g_successes;

    int active_count = 0;
    for (int s = 0; s < STREAM_COUNT; ++s) {
      bool active = parsed && snap.stream[s].slot[0].valid;
      if (g_stream_was_active[s] && !active)
        ++g_stream_dryup_observed[s];
      g_stream_was_active[s] = active;
      if (active)
        ++active_count;
    }

    // Drive the production sleep planner.
    SleepConfig cfg;
    if (!fo.ok)
      snap.api_ok = false;
    SleepDecision decision = planSleep(snap, t_cycle, cfg);
    bool no_data = decision.mode == Mode::DeepSleep &&
                   decision.seconds == (unsigned)cfg.no_data_sleep_s;
    if (no_data)
      ++g_no_data_sleep_decisions;

    // Drive the production render+display pipeline. We feed an empty
    // ScheduleSnapshot — this test isn't exercising the EFA hint path,
    // it's exercising the heap behaviour of renderFrame + planRefresh +
    // GxEPD2 partial/light-full across the evening transition. The
    // previous implementation skipped the whole render+display arm,
    // which kept the largest heap-consuming code path out of the soak.
    StreamSnapshot merged_for_render = snap;
    if (parsed) {
      ScheduleSnapshot empty_schedule;
      merged_for_render = mergeSlots(snap, empty_schedule, t_cycle);
    }
    RenderInput in{merged_for_render, OverlayKind::None};
    renderFrame(in, g_frame_new);
    bool prev_valid = (cycle > 1);
    RefreshConfig rc;
    RefreshDecision rd = planRefresh(
        g_frame_prev.data(), g_frame_new.data(), prev_valid, t_cycle,
        g_disp_meta.last_light_full, g_disp_meta.partial_count, rc);
    applyDisplayDecision(g_display, rd, g_frame_new.data(), g_disp_meta,
                         t_cycle);
    if (rd.kind == RefreshKind::Partial)
      ++g_partial_count;
    else if (rd.kind == RefreshKind::LightFull)
      ++g_light_full_count;
    else if (rd.kind == RefreshKind::DeepClean)
      ++g_deep_clean_count;
    // Roll g_frame_new into g_frame_prev for the next iteration's diff.
    std::memcpy(g_frame_prev.data(), g_frame_new.data(), Frame::bytes);

    if (cycle % 10 == 0 || active_count == 0) {
      struct tm local;
      localtime_r(&t_cycle, &local);
      Serial.printf("[evening] cycle=%d %02d:%02d active=%d no_data=%d "
                    "mode=%d sleep_s=%u heap=%u\n",
                    cycle, local.tm_hour, local.tm_min, active_count,
                    no_data ? 1 : 0, static_cast<int>(decision.mode),
                    decision.seconds, ESP.getFreeHeap());
    }

    while (g_clock.now() < t_cycle + CYCLE_INTERVAL_S)
      delay(500);
  }

  uint32_t final_heap = ESP.getFreeHeap();
  int total_dryups = 0;
  for (int s = 0; s < STREAM_COUNT; ++s)
    total_dryups += g_stream_dryup_observed[s];
  int heap_drift =
      static_cast<int>(g_initial_heap) - static_cast<int>(final_heap);
  Serial.printf("[evening] DONE successes=%d/%d total_dryups=%d "
                "no_data_decisions=%d heap_drift=%d\n",
                g_successes, CYCLES, total_dryups, g_no_data_sleep_decisions,
                heap_drift);

  TEST_ASSERT_GREATER_THAN_INT_MESSAGE(0, total_dryups,
                                       "evening: no stream observed drying up "
                                       "— wrong launch time or empty data");
  TEST_ASSERT_GREATER_THAN_INT_MESSAGE(0, g_no_data_sleep_decisions,
                                       "evening: NO_DATA_SLEEP_S was never "
                                       "decided — sleep-planner missed night");
  TEST_ASSERT_LESS_THAN_INT_MESSAGE(HEAP_LEAK_BUDGET_BYTES, heap_drift,
                                    "evening: heap drift exceeds 5 h budget");
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  UNITY_BEGIN();
  RUN_TEST(test_evening_start_conditions);
  RUN_TEST(test_dryup_and_sleep_decisions);
  UNITY_END();
}

void loop() {}
