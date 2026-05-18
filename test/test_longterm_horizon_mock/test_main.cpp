// Long-term horizon-mock firmware variant. Driven by the Python runner
// in `runner.py` — this firmware is NOT meant to be flashed standalone.
// Build via `pio run -e longterm-horizon-mock-firmware -t upload` with
// MOCK_API_BASE and MOCK_INSECURE provided as build-defines (the runner
// injects these via PLATFORMIO_BUILD_FLAGS).
//
// Per cycle the firmware fetches the mock URL, parses, counts realtime
// departures, evaluates a simplified "EFA hint" (active when realtime
// has been empty for 2 consecutive cycles), and emits one structured
// line on serial:
//     [engine] cycle=N realtime_count=K hint_active=B
// runner.py parses these lines and decides pass/fail.
//
// NOT registered as a `test_*` env: this isn't a Unity test — Unity has
// no concept of "external runner asserts on the log". Hence the env
// `longterm-horizon-mock-firmware` is firmware-only and not in the
// device test set. See test/README.md.

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <cstdio>
#include <cstring>

#include "config.h"
#include "data/wienerlinien_parse.h"
#include "hal/Esp32Display.h"
#include "hal/IPersistentStore.h"
#include "logic/display_apply.h"
#include "logic/refresh_planner.h"
#include "logic/slot_merger.h"
#include "render/layout.h"
#include "secrets.h"

#ifndef MOCK_API_BASE
#error                                                                         \
    "MOCK_API_BASE not defined — invoke via test/test_longterm_horizon_mock/runner.py, not pio run directly"
#endif

#ifndef MOCK_HINT_AFTER_EMPTY
#define MOCK_HINT_AFTER_EMPTY 2 // consecutive empty cycles before hint=ON
#endif

#ifndef MOCK_TOTAL_CYCLES
#define MOCK_TOTAL_CYCLES 9
#endif

#ifndef MOCK_CYCLE_INTERVAL_S
#define MOCK_CYCLE_INTERVAL_S 60
#endif

using namespace bustaferl;

namespace {

int g_consecutive_empty = 0;
bool g_hint_active = false;
Esp32Display g_display;
Frame g_frame_new;
Frame g_frame_prev;
PersistedMeta g_disp_meta;

std::string apiUrl() {
  std::string url = MOCK_API_BASE;
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

// Minimal plain-HTTP GET (the mock server is loopback, no TLS). Mirrors
// the production fetch semantics enough for parser exercise — the
// production HTTPS path is covered by `test_device_fetch` against the
// live API.
bool httpGetPlain(const std::string &url, std::string &out) {
  WiFiClient client;
  HTTPClient http;
  if (!http.begin(client, url.c_str()))
    return false;
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    return false;
  }
  out = http.getString().c_str();
  http.end();
  return true;
}

int countRealtime(const StreamSnapshot &snap) {
  int n = 0;
  for (int i = 0; i < STREAM_COUNT; ++i)
    if (snap.stream[i].filter_matched)
      ++n;
  return n;
}

} // namespace

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.printf("[engine] boot mock_api_base=%s\n", MOCK_API_BASE);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID_PRIMARY, WIFI_PASSWORD_PRIMARY);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - t0) < 15000) {
    delay(250);
  }
  Serial.printf("[engine] wifi connected=%d ip=%s\n",
                WiFi.status() == WL_CONNECTED,
                WiFi.localIP().toString().c_str());

  g_display.init();

  StreamFilter filters[STREAM_COUNT];
  buildFilters(filters);

  for (int cycle = 1; cycle <= MOCK_TOTAL_CYCLES; ++cycle) {
    uint32_t t_cycle_start = millis();
    std::string body;
    bool fetched = httpGetPlain(apiUrl(), body);
    int rt = 0;
    StreamSnapshot parsed_snap;
    bool parsed = false;
    if (fetched) {
      if (parseMonitorResponse(body, filters, parsed_snap)) {
        parsed = true;
        rt = countRealtime(parsed_snap);
      }
    }
    // Drive renderFrame + planRefresh + GxEPD2 partial/light-full so the
    // mock harness exercises the same display heap path as production
    // (Schritt 0a).
    if (parsed) {
      ScheduleSnapshot empty_schedule;
      time_t now_t = static_cast<time_t>(time(nullptr));
      StreamSnapshot merged_for_render =
          mergeSlots(parsed_snap, empty_schedule, now_t);
      RenderInput in{merged_for_render, OverlayKind::None};
      renderFrame(in, g_frame_new);
      bool prev_valid = (cycle > 1);
      RefreshConfig rc;
      RefreshDecision rd =
          planRefresh(g_frame_prev.data(), g_frame_new.data(), prev_valid,
                      now_t, g_disp_meta.last_light_full,
                      g_disp_meta.partial_count, rc);
      applyDisplayDecision(g_display, rd, g_frame_new.data(), g_disp_meta,
                           now_t);
      std::memcpy(g_frame_prev.data(), g_frame_new.data(), Frame::bytes);
    }
    if (rt == 0)
      ++g_consecutive_empty;
    else
      g_consecutive_empty = 0;
    if (g_consecutive_empty >= MOCK_HINT_AFTER_EMPTY)
      g_hint_active = true;
    if (rt >= 3)
      g_hint_active = false;

    Serial.printf("[engine] cycle=%d realtime_count=%d hint_active=%d "
                  "body=%u fetched=%d\n",
                  cycle, rt, g_hint_active ? 1 : 0,
                  static_cast<unsigned>(body.size()), fetched ? 1 : 0);
    Serial.flush();

    while ((millis() - t_cycle_start) <
           (uint32_t)MOCK_CYCLE_INTERVAL_S * 1000) {
      delay(100);
    }
  }

  Serial.println("[engine] DONE");
  Serial.flush();
}

void loop() {}
