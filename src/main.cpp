#ifndef NATIVE_BUILD

#include <Arduino.h>
#include <cstdio>
#include <string>

#include "config.h"
#include "data/wienerlinien_parse.h"
#include "hal/Esp32Clock.h"
#include "hal/Esp32Display.h"
#include "hal/Esp32Network.h"
#include "hal/Esp32PersistentStore.h"
#include "hal/Esp32Sleep.h"
#include "logic/boot_sequencer.h"
#include "logic/filter_health.h"
#include "logic/refresh_planner.h"
#include "logic/sleep_planner.h"
#include "logic/stale_policy.h"
#include "render/error_overlay.h"
#include "render/layout.h"
#include "render/rle.h"
#include "secrets.h"

using namespace bustaferl;

namespace {

Esp32Clock g_clock{NTP_SERVER, TZ_INFO};
Esp32Network g_net;
Esp32Sleep g_sleep;
Esp32PersistentStore g_store;
Esp32Display g_display;

Frame g_frame_new;
Frame g_frame_prev;

void buildFilters(StreamFilter (&f)[STREAM_COUNT]) {
  f[STREAM_58A_ATZ] = {RBL_TULL_ATZGERSDORF, LINE_58A, TOWARDS_58A_ATZ};
  f[STREAM_58A_HIETZING] = {RBL_TULL_HIETZING, LINE_58A, TOWARDS_58A_HIETZING};
  f[STREAM_58B_ATZ] = {RBL_ENDEMANN, LINE_58B, FILTER_TOWARDS_58B};
}

std::string apiUrl() {
  std::string url = WL_API_BASE;
  char buf[64];
  snprintf(buf, sizeof(buf), "&rbl=%d&rbl=%d&rbl=%d", RBL_TULL_ATZGERSDORF,
           RBL_TULL_HIETZING, RBL_ENDEMANN);
  url += buf;
  return url;
}

bool fetchSnapshot(StreamSnapshot &out) {
  std::string body;
  if (!g_net.httpGet(apiUrl(), body)) {
    Serial.println("[api] httpGet failed");
    return false;
  }
  StreamFilter filters[STREAM_COUNT];
  buildFilters(filters);
  bool parsed = parseMonitorResponse(body, filters, out);
  Serial.printf("[api] parse=%d api_ok=%d  streams: "
                "58A-Atz r=%d f=%d | 58A-Hie r=%d f=%d | 58B r=%d f=%d\n",
                parsed, out.api_ok,
                out.stream[STREAM_58A_ATZ].rbl_responded,
                out.stream[STREAM_58A_ATZ].filter_matched,
                out.stream[STREAM_58A_HIETZING].rbl_responded,
                out.stream[STREAM_58A_HIETZING].filter_matched,
                out.stream[STREAM_58B_ATZ].rbl_responded,
                out.stream[STREAM_58B_ATZ].filter_matched);
  return parsed;
}

void applyDisplayDecision(const RefreshDecision &d, const Frame &fb,
                          PersistedMeta &meta, time_t now) {
  switch (d.kind) {
  case RefreshKind::None:
    break;
  case RefreshKind::Partial:
    g_display.drawPartial(fb.data(), d.bbox);
    ++meta.partial_count;
    break;
  case RefreshKind::LightFull:
    g_display.lightFull(fb.data());
    meta.last_light_full = now;
    meta.partial_count = 0;
    break;
  case RefreshKind::DeepClean:
    g_display.deepClean(fb.data());
    meta.last_deep_clean = now;
    meta.last_light_full = now;
    meta.partial_count = 0;
    break;
  }
}

bool needsNightlyDeepClean(time_t now, time_t last) {
  if (last == 0)
    return true;
  return (now - last) >= 20 * 3600; // at least 20h since last clean
}

void registerWifiCredentials() {
  g_net.addAp(WIFI_SSID_PRIMARY, WIFI_PASSWORD_PRIMARY);
#ifdef WIFI_SSID_SECONDARY
  g_net.addAp(WIFI_SSID_SECONDARY, WIFI_PASSWORD_SECONDARY);
#endif
}

void renderAndPush(const StreamSnapshot &snap, OverlayKind overlay,
                   PersistedMeta &meta) {
  RenderInput in{snap, overlay, 3600};
  renderFrame(in, g_frame_new);

  bool prev_valid = meta.framebuffer_valid;
  if (prev_valid) {
    prev_valid = g_store.loadFramebuffer(g_frame_prev.data(), Frame::bytes) ==
                 Frame::bytes;
  }

  time_t now = g_clock.now();
  RefreshConfig rc;
  RefreshDecision d =
      planRefresh(g_frame_prev.data(), g_frame_new.data(), prev_valid, now,
                  meta.last_light_full, meta.partial_count, rc);

  applyDisplayDecision(d, g_frame_new, meta, now);

  bool saved = g_store.saveFramebuffer(g_frame_new.data(), Frame::bytes);
  meta.framebuffer_valid = saved;
}

void doSleepOrLoop(const SleepDecision &sd, const PersistedMeta &meta) {
  g_store.saveMeta(meta);
  if (sd.mode == Mode::DeepSleep) {
    Serial.printf("[sleep] deep sleep for %u s (next bus far away or no data)\n",
                  sd.seconds);
    g_sleep.deepSleep(sd.seconds);
  }
  Serial.printf("[sleep] staying active, light sleep for %u s\n",
                POLL_INTERVAL_S);
  g_sleep.lightSleep(POLL_INTERVAL_S);
}

void coldBootPath(PersistedMeta &meta) {
  Serial.printf("[boot] cold path, retry %u/%u\n", meta.cold_boot_retries,
                COLD_BOOT_MAX_RETRIES);
  BootConfig bc;
  bc.max_retries = COLD_BOOT_MAX_RETRIES;
  BootResult r = runColdBoot(g_net, g_clock, meta.cold_boot_retries, bc);
  Serial.printf("[boot] runColdBoot -> %s\n",
                r == BootResult::Ok ? "Ok"
                : r == BootResult::RetryLater ? "RetryLater"
                                              : "GiveUp");
  if (r == BootResult::RetryLater) {
    ++meta.cold_boot_retries;
    g_store.saveMeta(meta);
    Serial.println("[boot] retry later");
    g_sleep.deepSleep(COLD_BOOT_RETRY_S);
  } else if (r == BootResult::GiveUp) {
    Serial.println("[boot] give up");
    renderStartFailedFrame(g_frame_new);
    g_display.deepClean(g_frame_new.data());
    meta.cold_boot_retries = 0;
    meta.framebuffer_valid = false;
    g_store.saveMeta(meta);
    g_sleep.deepSleep(300); // try again in 5 min
  }
  meta.cold_boot_retries = 0;

  // First-ever render after a cold boot: deep clean for a known-good panel.
  StreamSnapshot snap;
  bool ok = fetchSnapshot(snap);
  if (ok) {
    meta.last_api_success = g_clock.now();
  }
  Serial.printf("[boot] fetch ok=%d, rendering and deep-cleaning panel\n", ok);
  RenderInput in{snap, OverlayKind::None, 3600};
  renderFrame(in, g_frame_new);
  g_display.deepClean(g_frame_new.data());
  meta.last_deep_clean = g_clock.now();
  meta.last_light_full = meta.last_deep_clean;
  meta.last_ntp_sync = g_clock.lastSync();
  g_store.saveFramebuffer(g_frame_new.data(), Frame::bytes);
  meta.framebuffer_valid = true;

  SleepConfig sc{WAKE_BEFORE_BUS_S, BOOT_MARGIN_S, ACTIVE_THRESHOLD_S,
                 NO_DATA_SLEEP_S};
  SleepDecision sd = planSleep(snap, g_clock.now(), sc);
  doSleepOrLoop(sd, meta);
}

void warmCyclePath(PersistedMeta &meta) {
  Serial.println("[warm] cycle start");
  if (!g_net.connect(10000)) {
    Serial.println("[warm] wifi down");
    // No data — keep showing last render, but flip to stale if old enough.
    if (isStale(meta.last_api_success, g_clock.now(), STALE_THRESHOLD_S)) {
      renderStaleFrame(g_frame_new);
      renderAndPush({}, OverlayKind::Stale, meta);
    }
    g_sleep.deepSleep(POLL_INTERVAL_S);
  }

  // ESP32 deep sleep loses the system clock — now() returns seconds since
  // boot, not Unix epoch. Without this guard, the periodic NTP check below
  // (a signed subtraction) underflows to a huge negative and never fires,
  // leaving planSleep to compute a 50-year deep sleep against a bogus now.
  if (g_clock.now() < 1700000000) {
    Serial.printf("[warm] clock unsynced (now=%lld), forcing NTP\n",
                  static_cast<long long>(g_clock.now()));
    if (g_clock.ntpSync()) {
      meta.last_ntp_sync = g_clock.now();
      Serial.printf("[warm] NTP recovered: now=%lld\n",
                    static_cast<long long>(g_clock.now()));
    } else {
      Serial.println("[warm] NTP failed, retrying after cold-boot interval");
      g_sleep.deepSleep(COLD_BOOT_RETRY_S);
    }
  }

  // Periodic NTP refresh.
  if (g_clock.now() - meta.last_ntp_sync > NTP_INTERVAL_S) {
    if (g_clock.ntpSync())
      meta.last_ntp_sync = g_clock.now();
  }

  StreamSnapshot snap;
  bool ok = fetchSnapshot(snap);
  time_t now = g_clock.now();

  static FilterHealth fh(FILTER_HEALTH_DEAD_AFTER);
  fh.setStreak(meta.filter_miss_streak);

  OverlayKind overlay = OverlayKind::None;
  if (ok) {
    meta.last_api_success = now;
    fh.recordCall(snap.stream[STREAM_58B_ATZ].rbl_responded,
                  snap.stream[STREAM_58B_ATZ].filter_matched);
    meta.filter_miss_streak = fh.streak();
    if (fh.isDead())
      overlay = OverlayKind::FilterDead;
  } else if (isStale(meta.last_api_success, now, STALE_THRESHOLD_S)) {
    overlay = OverlayKind::Stale;
    snap = StreamSnapshot{};
  }

  // Nightly deep clean: if next sleep would be long and we haven't cleaned
  // in 20h, do it now.
  SleepConfig sc{WAKE_BEFORE_BUS_S, BOOT_MARGIN_S, ACTIVE_THRESHOLD_S,
                 NO_DATA_SLEEP_S};
  SleepDecision pre = planSleep(snap, now, sc);
  bool nightly = pre.mode == Mode::DeepSleep && pre.seconds > 4 * 3600 &&
                 needsNightlyDeepClean(now, meta.last_deep_clean);

  if (nightly) {
    RenderInput in{snap, overlay, 3600};
    renderFrame(in, g_frame_new);
    g_display.deepClean(g_frame_new.data());
    meta.last_deep_clean = now;
    meta.last_light_full = now;
    meta.partial_count = 0;
    g_store.saveFramebuffer(g_frame_new.data(), Frame::bytes);
    meta.framebuffer_valid = true;
  } else {
    renderAndPush(snap, overlay, meta);
  }

  doSleepOrLoop(pre, meta);
}

} // namespace

void setup() {
  Serial.begin(115200);
  delay(100);

  registerWifiCredentials();
  g_display.init();

  PersistedMeta meta = g_store.loadMeta();

  if (g_sleep.wakeupCause() == WakeCause::ColdBoot) {
    Serial.println("[boot] cold");
    coldBootPath(meta);
  } else {
    Serial.println("[boot] warm");
    warmCyclePath(meta);
  }
}

void loop() {
  // Active-phase polling: warmCyclePath set us up for
  // lightSleep(POLL_INTERVAL_S) and we get here when it returns. Just rerun the
  // warm cycle.
  PersistedMeta meta = g_store.loadMeta();
  warmCyclePath(meta);
}

#endif // NATIVE_BUILD
