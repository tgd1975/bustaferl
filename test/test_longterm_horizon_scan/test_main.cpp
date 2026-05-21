// Long-term Horizon-Scan test (~90 min, daytime). Drives the production
// fetch loop against the live API at peak data availability and watches
// the rolling-window cliff: a departure that entered the horizon at
// cycle K should leave the horizon at the cycle when its ETA decrements
// past zero — not earlier (premature drop), not later (zombie slot).
//
// Per-cycle telemetry: for each stream, log the first slot's ETA. A
// monotonically decreasing ETA across cycles (modulo new arrivals) is
// the invariant. Any cycle where a stream's ETA jumps UPWARD without a
// new departure arriving is a regression.
//
// Run via `make test-longterm-horizon-scan` (env:longterm-horizon-scan).
// Start during daytime (08:00–17:00) when realtime feeds are densest.

#include "config.h"
#include "data/wienerlinien_parse.h"
#include "hal/Esp32Clock.h"
#include "hal/Esp32Display.h"
#include "hal/Esp32Network.h"
#include "hal/IPersistentStore.h"
#include "logic/api_fetcher.h"
#include "logic/display_apply.h"
#include "logic/filter_builder.h"
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

constexpr int CYCLES = 90;
constexpr int CYCLE_INTERVAL_S = 60; // ~90 min wallclock
constexpr int MIN_SUCCESS_PCT = 90;  // daytime API is reliable

Esp32Network g_net;
Esp32Clock g_clock{NTP_SERVER, TZ_INFO};
Esp32Display g_display;
Frame g_frame_new;
Frame g_frame_prev;
PersistedMeta g_disp_meta;

int g_successes = 0;
int g_upward_jumps = 0; // ETA jumped up without a plausible new arrival
time_t g_prev_first_slot[STREAM_COUNT] = {0};

std::string apiUrl() {
  std::string url = WL_API_BASE;
  char buf[96];
  std::snprintf(buf, sizeof(buf), "&stopId=%d&stopId=%d&stopId=%d",
                RBL_TULL_ATZGERSDORF, RBL_TULL_HIETZING, RBL_ENDEMANN);
  url += buf;
  return url;
}

const char *streamName(int i) {
  switch (i) {
  case STREAM_58A_ATZ:
    return "58A-Atz";
  case STREAM_58A_HIETZING:
    return "58A-Hie";
  case STREAM_58B_ATZ:
    return "58B";
  case STREAM_SBAHN_HBF:
    return "SBahn";
  }
  return "?";
}

} // namespace

void test_setup_wifi_and_clock(void) {
  g_net.addAp(WIFI_SSID_PRIMARY, WIFI_PASSWORD_PRIMARY);
#ifdef WIFI_SSID_SECONDARY
  g_net.addAp(WIFI_SSID_SECONDARY, WIFI_PASSWORD_SECONDARY);
#endif
  TEST_ASSERT_TRUE_MESSAGE(g_net.connect(15000),
                           "horizon_scan: initial WiFi failed");
  TEST_ASSERT_TRUE_MESSAGE(g_clock.ntpSync(),
                           "horizon_scan: initial NTP sync failed");
  g_display.init();
}

void test_horizon_cliff_loop(void) {
  StreamFilter filters[STREAM_COUNT];
  buildStreamFilters(filters);

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

    for (int s = 0; s < STREAM_COUNT; ++s) {
      if (!snap.stream[s].filter_matched)
        continue;
      const Departure &d = snap.stream[s].slot[0];
      if (!d.valid)
        continue;
      time_t first = d.when;
      time_t prev = g_prev_first_slot[s];
      int delta = prev ? static_cast<int>(first - prev) : 0;
      Serial.printf("[horizon] cycle=%d %s first_eta_s=%d prev_eta_s=%d "
                    "delta=%d\n",
                    cycle, streamName(s), static_cast<int>(first - t_cycle),
                    prev ? static_cast<int>(prev - t_cycle) : -1, delta);
      // Upward delta > CYCLE_INTERVAL_S means the previous "first slot"
      // departed and the new first slot is the next bus — that's a
      // cliff, not a regression. Upward delta <= CYCLE_INTERVAL_S
      // without that being the case means the data jittered backwards.
      if (delta > CYCLE_INTERVAL_S * 4) {
        // expected cliff: previous bus left horizon, next one is N min away
      } else if (delta > 0) {
        ++g_upward_jumps;
        Serial.printf("[horizon] WARN cycle=%d %s upward jump delta=%d\n",
                      cycle, streamName(s), delta);
      }
      g_prev_first_slot[s] = first;
    }

    // Drive renderFrame + planRefresh + GxEPD2 partial/light-full on
    // every cycle so heap behaviour of the display pipeline is visible
    // in this rolling-window test (Schritt 0a).
    if (parsed) {
      ScheduleSnapshot empty_schedule;
      StreamSnapshot merged_for_render =
          mergeSlots(snap, empty_schedule, t_cycle);
      RenderInput in;
      in.state = DisplayState::Normal;
      in.snapshot = merged_for_render;
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

    while (g_clock.now() < t_cycle + CYCLE_INTERVAL_S)
      delay(500);
  }

  Serial.printf("[horizon] DONE successes=%d/%d upward_jumps=%d\n", g_successes,
                CYCLES, g_upward_jumps);
  TEST_ASSERT_GREATER_OR_EQUAL_INT_MESSAGE(
      (CYCLES * MIN_SUCCESS_PCT) / 100, g_successes,
      "horizon_scan: success rate below threshold");
  TEST_ASSERT_LESS_THAN_INT_MESSAGE(
      10, g_upward_jumps,
      "horizon_scan: too many upward ETA jumps — rolling-window logic suspect");
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  UNITY_BEGIN();
  RUN_TEST(test_setup_wifi_and_clock);
  RUN_TEST(test_horizon_cliff_loop);
  UNITY_END();
}

void loop() {}
