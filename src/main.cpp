#ifndef NATIVE_BUILD

#include "config.h"
#include "data/ScheduleHint.h"
#include "data/efa_parse.h"
#include "data/wienerlinien_parse.h"
#include "hal/Esp32Clock.h"
#include "hal/Esp32Display.h"
#include "hal/Esp32Network.h"
#include "hal/Esp32PersistentStore.h"
#include "hal/Esp32Sleep.h"
#include "logic/api_fetcher.h"
#include "logic/boot_sequencer.h"
#include "logic/display_apply.h"
#include "logic/filter_builder.h"
#include "logic/filter_health.h"
#include "logic/refresh_planner.h"
#include "logic/schedule_fetcher.h"
#include "logic/schedule_refresh.h"
#include "logic/sleep_planner.h"
#include "logic/slot_merger.h"
#include "logic/snapshot_fetcher.h"
#include "logic/snapshot_logger.h"
#include "logic/stale_policy.h"
#include "render/error_overlay.h"
#include "render/layout.h"
#include "render/rle.h"
#include "secrets.h"

#include <Arduino.h>
#include <cstdio>
#include <ctime>
#include <string>

using namespace bustaferl;

namespace {

Esp32Clock g_clock{NTP_SERVER, TZ_INFO};
Esp32Network g_net;
Esp32Sleep g_sleep;
Esp32PersistentStore g_store;
Esp32Display g_display;

Frame g_frame_new;
Frame g_frame_prev;

bool fetchSnapshotAndLog(StreamSnapshot &out) {
  StreamFilter filters[STREAM_COUNT];
  buildStreamFilters(filters);
  FetchSummary summary;
  bool ok = fetchSnapshot(g_net, WL_API_BASE, filters, out, summary);
  Serial.print(
      formatSnapshotSummary(out, summary.total_batches, summary.failed_batches)
          .c_str());
  return ok;
}

void registerWifiCredentials() {
  g_net.addAp(WIFI_SSID_PRIMARY, WIFI_PASSWORD_PRIMARY);
#ifdef WIFI_SSID_SECONDARY
  g_net.addAp(WIFI_SSID_SECONDARY, WIFI_PASSWORD_SECONDARY);
#endif
}

// One EFA pass per distinct DIVA. Returns true if at least one call yielded
// usable data; partial success is good enough to update the snapshot (any
// streams that did not get fresh data keep their previous hint values via
// the load-modify-save flow in the callers).
bool refreshSchedule(ScheduleSnapshot &out) {
  ScheduleStreamFilter sf[STREAM_COUNT];
  buildScheduleFilters(sf);
  ScheduleFetchConfig cfg;
  cfg.endpoint_base = WL_EFA_DM_BASE;
  time_t now = g_clock.now();
  ScheduleFetchResult r = fetchSchedule(g_net, now, sf, cfg);
  Serial.printf("[sched] calls=%d failed=%d ok=%d\n", r.calls_attempted,
                r.calls_failed, r.ok);
  return applyScheduleFetchResult(r, now, out);
}

void renderAndPush(const StreamSnapshot &snap, OverlayKind overlay,
                   PersistedMeta &meta, const ScheduleSnapshot &schedule) {
  // Stale overlay must keep showing "??:??" everywhere — do not let scheduled
  // hints leak through when the realtime data is untrustworthy.
  StreamSnapshot merged = (overlay == OverlayKind::Stale)
                              ? snap
                              : mergeSlots(snap, schedule, g_clock.now());
  RenderInput in{merged, overlay};
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

  applyDisplayDecision(g_display, d, g_frame_new.data(), meta, now);

  bool saved = g_store.saveFramebuffer(g_frame_new.data(), Frame::bytes);
  meta.framebuffer_valid = saved;
}

void doSleepOrLoop(const SleepDecision &sd, const PersistedMeta &meta) {
  g_store.saveMeta(meta);
  if (sd.mode == Mode::DeepSleep) {
    Serial.printf(
        "[sleep] deep sleep for %u s (next bus far away or no data)\n",
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
  Serial.printf("[boot] runColdBoot -> %s\n", r == BootResult::Ok ? "Ok"
                                              : r == BootResult::RetryLater
                                                  ? "RetryLater"
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
  bool ok = fetchSnapshotAndLog(snap);
  if (ok) {
    meta.last_api_success = g_clock.now();
  }
  // Schedule fetch is best-effort: failure leaves `schedule.fetched_at = 0`
  // and the renderer falls back to pure realtime behaviour.
  ScheduleSnapshot schedule = g_store.loadSchedule();
  if (refreshSchedule(schedule)) {
    g_store.saveSchedule(schedule);
  }
  Serial.printf("[boot] fetch ok=%d, rendering and deep-cleaning panel\n", ok);
  StreamSnapshot merged = mergeSlots(snap, schedule, g_clock.now());
  RenderInput in{merged, OverlayKind::None};
  renderFrame(in, g_frame_new);
  g_display.deepClean(g_frame_new.data());
  meta.last_deep_clean = g_clock.now();
  meta.last_light_full = meta.last_deep_clean;
  meta.last_ntp_sync = g_clock.lastSync();
  g_store.saveFramebuffer(g_frame_new.data(), Frame::bytes);
  meta.framebuffer_valid = true;

  SleepConfig sc{WAKE_BEFORE_BUS_S, BOOT_MARGIN_S, ACTIVE_THRESHOLD_S,
                 NO_DATA_SLEEP_S, API_FAILURE_RETRY_S};
  // planSleep wants the merged view: if realtime has nothing but hints fill
  // a slot, the next bus is the hint's time — sleep until then, not until
  // the conservative "no data" interval.
  SleepDecision sd = planSleep(merged, g_clock.now(), sc);
  doSleepOrLoop(sd, meta);
}

void warmCyclePath(PersistedMeta &meta) {
  Serial.println("[warm] cycle start");
  ScheduleSnapshot schedule = g_store.loadSchedule();
  if (!g_net.connect(10000)) {
    Serial.println("[warm] wifi down");
    // No data — keep showing last render, but flip to stale if old enough.
    if (isStale(meta.last_api_success, g_clock.now(), STALE_THRESHOLD_S)) {
      renderStaleFrame(g_frame_new);
      renderAndPush({}, OverlayKind::Stale, meta, schedule);
    }
    g_sleep.deepSleep(POLL_INTERVAL_S);
    return; // deepSleep doesn't return on hardware, but be explicit
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
  bool ok = fetchSnapshotAndLog(snap);
  time_t now = g_clock.now();

  static FilterHealth fh(FILTER_HEALTH_DEAD_AFTER);
  fh.setStreak(meta.filter_miss_streak);

  OverlayKind overlay = OverlayKind::None;
  if (ok) {
    meta.last_api_success = now;

    // FilterHealth only gets a signal when at least one of our streams has
    // actual departures. During the nightly Betriebstag-Pause the API
    // returns matching RBLs/lines but with empty `departures` arrays — that
    // would otherwise read as "filter responded but didn't match" for
    // hours and falsely trip FilterDead. Treat "no service anywhere" as no
    // signal instead of as a streak.
    bool any_service = false;
    for (int s = 0; s < STREAM_COUNT; ++s) {
      if (snap.stream[s].slot[0].valid) {
        any_service = true;
        break;
      }
    }
    if (any_service) {
      fh.recordCall(snap.stream[STREAM_58B_ATZ].endpoint_responded,
                    snap.stream[STREAM_58B_ATZ].filter_matched);
      meta.filter_miss_streak = fh.streak();
      if (fh.isDead())
        overlay = OverlayKind::FilterDead;
    }
  } else if (isStale(meta.last_api_success, now, STALE_THRESHOLD_S)) {
    overlay = OverlayKind::Stale;
    snap = StreamSnapshot{};
  }

  // Schedule refresh: piggyback on the nightly slot (WiFi is up, we're about
  // to sleep long). Also forced if the on-RTC snapshot is stale-old.
  if (needScheduleRefresh(schedule, now)) {
    if (refreshSchedule(schedule)) {
      g_store.saveSchedule(schedule);
    }
  }

  // Nightly deep clean: if next sleep would be long and we haven't cleaned
  // in 20h, do it now. Use the merged view for sleep planning so a fresh
  // morning hint shortens what would otherwise be a generic NO_DATA sleep.
  StreamSnapshot merged =
      (overlay == OverlayKind::Stale) ? snap : mergeSlots(snap, schedule, now);
  SleepConfig sc{WAKE_BEFORE_BUS_S, BOOT_MARGIN_S, ACTIVE_THRESHOLD_S,
                 NO_DATA_SLEEP_S, API_FAILURE_RETRY_S};
  SleepDecision pre = planSleep(merged, now, sc);
  bool nightly = pre.mode == Mode::DeepSleep && pre.seconds > 4 * 3600 &&
                 needsNightlyDeepClean(now, meta.last_deep_clean, 20 * 3600);

  if (nightly) {
    RenderInput in{merged, overlay};
    renderFrame(in, g_frame_new);
    g_display.deepClean(g_frame_new.data());
    meta.last_deep_clean = now;
    meta.last_light_full = now;
    meta.partial_count = 0;
    g_store.saveFramebuffer(g_frame_new.data(), Frame::bytes);
    meta.framebuffer_valid = true;
  } else if (ok || overlay != OverlayKind::None) {
    renderAndPush(snap, overlay, meta, schedule);
  } else {
    // Transient fetch failure (not yet stale): keep showing the last good
    // frame. Re-rendering with an empty snap would flicker every slot to
    // "--:--" for one cycle and back.
    Serial.println("[warm] fetch failed pre-stale — keeping last frame");
  }

  doSleepOrLoop(pre, meta);
}

enum class ButtonPress { None, Short, Long };

// Block until the boot button is released; classify as short or long based
// on BTN_LONG_PRESS_MS. Assumes the button is currently held LOW (we got
// here because of an EXT0/GPIO wake or a positive poll). If it's already
// HIGH on entry, treat as a transient press (Short).
ButtonPress measureButtonPress() {
  pinMode(BTN_BOOT_PIN, INPUT_PULLUP);
  delay(20); // debounce settle
  if (digitalRead(BTN_BOOT_PIN) == HIGH) {
    return ButtonPress::Short;
  }
  uint32_t t0 = millis();
  bool long_latched = false;
  while (digitalRead(BTN_BOOT_PIN) == LOW) {
    if (!long_latched && millis() - t0 >= BTN_LONG_PRESS_MS) {
      long_latched = true;
      Serial.println("[btn] long-press threshold reached, waiting for release");
    }
    delay(10);
  }
  Serial.printf("[btn] released after %u ms\n",
                static_cast<unsigned>(millis() - t0));
  return long_latched ? ButtonPress::Long : ButtonPress::Short;
}

// Long-press action: flush the panel with a B/W deep clean and redraw the
// last good framebuffer. If we have nothing persisted, render an empty
// frame and clean. Caller still runs the regular warm cycle afterwards to
// fetch fresh data.
void runBwReset(PersistedMeta &meta) {
  Serial.println("[btn] BW reset + redraw");
  bool have_fb =
      meta.framebuffer_valid &&
      g_store.loadFramebuffer(g_frame_new.data(), Frame::bytes) == Frame::bytes;
  if (!have_fb) {
    Serial.println("[btn] no valid framebuffer — rendering empty for clean");
    StreamSnapshot snap;
    RenderInput in{snap, OverlayKind::None};
    renderFrame(in, g_frame_new);
  }
  g_display.deepClean(g_frame_new.data());
  if (!have_fb) {
    g_store.saveFramebuffer(g_frame_new.data(), Frame::bytes);
    meta.framebuffer_valid = true;
  }
  time_t now = g_clock.now();
  meta.last_deep_clean = now;
  meta.last_light_full = now;
  meta.partial_count = 0;
  g_store.saveMeta(meta);
}

// Poll the button right now (used in loop() after light-sleep wake) and,
// if a press is in progress, classify + dispatch. Returns the classified
// press for the caller's logging convenience.
ButtonPress handleButtonIfPressed(PersistedMeta &meta) {
  pinMode(BTN_BOOT_PIN, INPUT_PULLUP);
  delay(5);
  if (digitalRead(BTN_BOOT_PIN) != LOW) {
    return ButtonPress::None;
  }
  Serial.println("[btn] press detected");
  ButtonPress p = measureButtonPress();
  if (p == ButtonPress::Long) {
    runBwReset(meta);
  } else {
    Serial.println("[btn] short — proceed with update");
  }
  return p;
}

} // namespace

void setup() {
  Serial.begin(115200);
  delay(100);

  pinMode(BTN_BOOT_PIN, INPUT_PULLUP);

  registerWifiCredentials();
  g_display.init();

  PersistedMeta meta = g_store.loadMeta();

  WakeCause cause = g_sleep.wakeupCause();
  if (cause == WakeCause::ColdBoot) {
    Serial.println("[boot] cold");
    coldBootPath(meta);
  } else if (cause == WakeCause::Button) {
    Serial.println("[boot] button-wake");
    ButtonPress p = measureButtonPress();
    if (p == ButtonPress::Long) {
      runBwReset(meta);
    } else {
      Serial.println("[btn] short — proceed with update");
    }
    warmCyclePath(meta);
  } else {
    Serial.println("[boot] warm");
    warmCyclePath(meta);
  }
}

void loop() {
  // Active-phase polling: warmCyclePath set us up for
  // lightSleep(POLL_INTERVAL_S) and we get here when it returns — either
  // on timer, or because the boot button was pressed (configured as a GPIO
  // wake source). Check the button first so a long press during active
  // mode still resets the panel.
  PersistedMeta meta = g_store.loadMeta();
  handleButtonIfPressed(meta);
  warmCyclePath(meta);
}

#endif // NATIVE_BUILD
