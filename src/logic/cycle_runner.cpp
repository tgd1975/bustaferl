#include "logic/cycle_runner.h"

#include "data/StreamSnapshot.h"
#include "logic/boot_sequencer.h"
#include "logic/button_classifier.h"
#include "logic/display_apply.h"
#include "logic/filter_builder.h"
#include "logic/filter_health.h"
#include "logic/refresh_planner.h"
#include "logic/render_input.h"
#include "logic/rescue_policy.h"
#include "logic/schedule_fetcher.h"
#include "logic/schedule_refresh.h"
#include "logic/sleep_planner.h"
#include "logic/slot_merger.h"
#include "logic/snapshot_fetcher.h"
#include "logic/snapshot_logger.h"
#include "logic/stale_policy.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>

#ifndef NATIVE_BUILD
#include <Arduino.h>
#define CYCLE_LOG(...) Serial.printf(__VA_ARGS__)
#define CYCLE_LOG_LN(s) Serial.println(s)
#define CYCLE_LOG_STR(s) Serial.print(s)
#else
#define CYCLE_LOG(...) ((void)0)
#define CYCLE_LOG_LN(s) ((void)0)
#define CYCLE_LOG_STR(s) ((void)0)
#endif

namespace bustaferl {

namespace {

bool fetchSnapshotAndLog(CycleDeps &deps, StreamSnapshot &out,
                         FetchSummary &summary) {
  StreamFilter filters[STREAM_COUNT];
  buildStreamFilters(filters);
  OebbStreamFilter oebb = buildOebbFilter();
  bool ok = fetchSnapshot(deps.net, deps.cfg.api_base, deps.cfg.mgate_url,
                          filters, oebb, out, summary);
  CYCLE_LOG_STR(
      formatSnapshotSummary(out, summary.total_batches, summary.failed_batches)
          .c_str());
  return ok;
}

// One EFA pass per distinct DIVA. Returns true if at least one call yielded
// usable data; partial success is good enough to update the snapshot.
bool refreshSchedule(CycleDeps &deps, ScheduleSnapshot &out) {
  ScheduleStreamFilter sf[STREAM_COUNT];
  buildScheduleFilters(sf);
  ScheduleFetchConfig fc;
  fc.endpoint_base = deps.cfg.efa_base;
  time_t now = deps.clock.now();
  ScheduleFetchResult r = fetchSchedule(deps.net, now, sf, fc);
  CYCLE_LOG("[sched] calls=%d failed=%d ok=%d\n", r.calls_attempted,
            r.calls_failed, r.ok);
  return applyScheduleFetchResult(r, now, out);
}

// Shared fetch+merge result. Hands back the realtime snapshot, the merged view
// for planSleep, the global overlay (None / Stale), the two per-section
// banner flags (58B filter dead, ÖBB auth dead), and the batch summary the
// rescue logic reads to tell "complete" from "partial" fetches.
struct FetchCycleResult {
  StreamSnapshot snap;
  StreamSnapshot merged;
  FetchSummary summary;
  OverlayKind overlay = OverlayKind::None;
  bool filter_dead_58b = false; // section-2 inline banner (58B filter dead)
  bool oebb_auth_dead = false;  // section-3 inline banner (ÖBB auth dead)
  bool fetched_ok = false;
};

// Complete ⇔ every batch (OGD + ÖBB) came back parsable. `fetched_ok` alone
// is weaker — it is true as soon as one batch survives, which is exactly the
// "some rows show --:-- although the line is running" case the rescue targets.
bool fetchComplete(const FetchSummary &s) {
  return s.total_batches > 0 && s.failed_batches == 0;
}

void renderAndPush(CycleDeps &deps, const FetchCycleResult &fc,
                   PersistedMeta &meta, const ScheduleSnapshot &schedule) {
  time_t now = deps.clock.now();
  RenderInput in = composeRenderInput(fc.snap, schedule, fc.overlay, now);
  in.filter_dead_58b = fc.filter_dead_58b;
  in.oebb_auth_dead = fc.oebb_auth_dead;
  deps.renderer.render(in, deps.curr);

  bool prev_valid = meta.framebuffer_valid;
  if (prev_valid) {
    prev_valid = deps.store.loadFramebuffer(deps.prev.data(), Frame::bytes) ==
                 Frame::bytes;
  }

  RefreshConfig rc;
  RefreshDecision d =
      planRefresh(deps.prev.data(), deps.curr.data(), prev_valid, now,
                  meta.last_light_full, meta.partial_count, rc);

  applyDisplayDecision(deps.display, d, deps.curr.data(), meta, now);

  bool saved = deps.store.saveFramebuffer(deps.curr.data(), Frame::bytes);
  meta.framebuffer_valid = saved;
}

void doSleepOrLoop(CycleDeps &deps, const SleepDecision &sd,
                   const PersistedMeta &meta) {
  deps.store.saveMeta(meta);
  if (sd.mode == Mode::DeepSleep) {
    CYCLE_LOG("[sleep] deep sleep for %u s (next bus far away or no data)\n",
              sd.seconds);
    deps.sleep.deepSleep(sd.seconds);
    return;
  }
  CYCLE_LOG("[sleep] staying active, light sleep for %u s\n",
            deps.cfg.poll_interval_s);
  deps.sleep.lightSleep(deps.cfg.poll_interval_s);
}

SleepConfig makeSleepConfig(const CycleConfig &cfg) {
  return SleepConfig{cfg.wake_before_bus_s, cfg.boot_margin_s,
                     cfg.active_threshold_s, cfg.no_data_sleep_s,
                     cfg.api_failure_retry_s};
}

FetchCycleResult doFetchCycle(CycleDeps &deps, PersistedMeta &meta,
                              const ScheduleSnapshot &schedule) {
  FetchCycleResult r;
  FetchSummary &summary = r.summary;
  r.fetched_ok = fetchSnapshotAndLog(deps, r.snap, summary);
  time_t now = deps.clock.now();

  FilterHealth fh(deps.cfg.filter_health_dead_after);
  fh.setStreak(meta.filter_miss_streak);

  if (r.fetched_ok) {
    meta.last_api_success = now;
    // FilterHealth only gets a signal when at least one of our streams has
    // actual departures. During the nightly Betriebstag-Pause the API
    // returns matching RBLs/lines but with empty `departures` arrays — that
    // would otherwise read as "filter responded but didn't match" for hours
    // and falsely trip the banner. Treat "no service anywhere" as no signal.
    const bool any_service =
        std::any_of(std::begin(r.snap.stream), std::end(r.snap.stream),
                    [](const StreamData &s) { return s.slot[0].valid; });
    if (any_service) {
      fh.recordCall(r.snap.stream[STREAM_58B_ATZ].endpoint_responded,
                    r.snap.stream[STREAM_58B_ATZ].filter_matched);
      meta.filter_miss_streak = fh.streak();
      r.filter_dead_58b = fh.isDead();
    }
    // ÖBB auth-health: a signal only when the POST actually returned (2xx).
    // err != "OK" (e.g. a stale AID) leaves endpoint_responded false → the
    // streak grows; a clean response resets it.
    if (summary.oebb_http_ok) {
      if (r.snap.stream[STREAM_SBAHN_HBF].endpoint_responded) {
        meta.oebb_auth_miss_streak = 0;
      } else if (meta.oebb_auth_miss_streak <
                 std::numeric_limits<uint8_t>::max()) {
        ++meta.oebb_auth_miss_streak;
      }
      r.oebb_auth_dead =
          meta.oebb_auth_miss_streak >= deps.cfg.filter_health_dead_after;
    }
  } else if (isStale(meta.last_api_success, now, deps.cfg.stale_threshold_s)) {
    r.overlay = OverlayKind::Stale;
    r.snap = StreamSnapshot{};
  }

  r.merged = (r.overlay == OverlayKind::Stale)
                 ? r.snap
                 : mergeSlots(r.snap, schedule, now);
  return r;
}

// Re-merge after a successful schedule refresh so planSleep sizes the next
// sleep with the fresh hints (the stale case renders and plans without hints
// on purpose — doFetchCycle already pinned merged = snap there).
void remergeAfterRefresh(FetchCycleResult &fc, const ScheduleSnapshot &schedule,
                         time_t now) {
  if (fc.overlay != OverlayKind::Stale) {
    fc.merged = mergeSlots(fc.snap, schedule, now);
  }
}

// Incomplete-snapshot rescue: the cycle already rendered what it had; keep
// re-fetching inside the [window_start, window_end] window after that display
// update and push one extra refresh as soon as a complete snapshot arrives.
// The window's lower bound keeps two panel updates from landing back-to-back.
// The button is never polled here — a press during a rescue merely ends the
// pacing lightSleep early; the running update finishes, it is not interrupted.
// Returns true iff a complete snapshot was fetched and pushed (fc replaced).
bool runRescueFetch(CycleDeps &deps, PersistedMeta &meta,
                    const ScheduleSnapshot &schedule, FetchCycleResult &fc,
                    time_t anchored_at) {
  RescueConfig rc;
  rc.window_start_s = deps.cfg.rescue_window_start_s;
  rc.window_end_s = deps.cfg.rescue_window_end_s;
  rc.retry_pause_s = deps.cfg.rescue_retry_pause_s;

  int attempts = 0;
  // Wait iterations are bounded too, so a clock that does not advance (host
  // fakes, RTC anomaly) cannot spin this loop forever.
  int waits = 0;
  while (attempts < deps.cfg.rescue_max_attempts &&
         waits <= deps.cfg.rescue_max_attempts) {
    unsigned wait_s = 0;
    RescueStep step = nextRescueStep(deps.clock.now(), anchored_at, rc, wait_s);
    if (step == RescueStep::Stop)
      return false;
    if (step == RescueStep::Wait) {
      ++waits;
      deps.sleep.lightSleep(wait_s);
      continue;
    }
    ++attempts;
    CYCLE_LOG("[rescue] attempt %d/%d\n", attempts,
              deps.cfg.rescue_max_attempts);
    FetchCycleResult retry = doFetchCycle(deps, meta, schedule);
    if (retry.fetched_ok && fetchComplete(retry.summary)) {
      CYCLE_LOG_LN("[rescue] complete snapshot — pushing extra refresh");
      fc = retry;
      renderAndPush(deps, fc, meta, schedule);
      return true;
    }
    deps.sleep.lightSleep(static_cast<unsigned>(rc.retry_pause_s));
  }
  return false;
}

// Captured at cold-cycle entry, before this boot mutates meta/schedule —
// feeds the boot-check dashboard (CONCEPT.md §8).
struct BootContext {
  bool meta_restored = false;
  bool frame_restored = false;
  bool schedule_restored = false;
  int attempt = 1; // 1-based attempt WLAN+NTP came up on
};

BootReport buildBootReport(CycleDeps &deps, const StreamSnapshot &snap,
                           const FetchSummary &summary,
                           const ScheduleSnapshot &schedule,
                           const BootContext &ctx) {
  BootReport r;
  r.valid = true;
  NetInfo ni;
  r.has_net_info = deps.net.connectionInfo(ni);
  if (r.has_net_info) {
    std::memcpy(r.ssid, ni.ssid, sizeof(r.ssid));
    std::memcpy(r.ip, ni.ip, sizeof(r.ip));
    r.rssi_dbm = ni.rssi_dbm;
  }
  r.now = deps.clock.now();
  r.ntp_ok = deps.clock.isSynced();
  r.snap = snap;
  r.oebb_http_ok = summary.oebb_http_ok;
  r.batches_total = summary.total_batches;
  r.batches_failed = summary.failed_batches;
  r.batches_retried = summary.retried_batches;
  r.schedule_ok = schedule.fetched_at != 0;
  r.hint_streams_expected = OGD_STREAM_COUNT;
  for (int i = 0; i < OGD_STREAM_COUNT; ++i) {
    const ScheduleHint &h = schedule.hint[i];
    if (h.last_today != 0 || h.next_today[1] != 0 || h.first_tomorrow[0] != 0)
      ++r.hint_streams_loaded;
  }
  r.meta_restored = ctx.meta_restored;
  r.frame_restored = ctx.frame_restored;
  r.schedule_restored = ctx.schedule_restored;
  r.boot_attempt = ctx.attempt;
  r.boot_attempts_max = deps.cfg.cold_boot_max_retries;
  r.uptime_ms = deps.clock.ticksMs();
  r.show_s = deps.cfg.boot_info_show_s;
  return r;
}

// Boot-check dashboard: one light-full render, then a lightSleep the boot
// button may cut short (GPIO wake) — afterwards the regular first frame
// replaces it. Refresh bookkeeping is untouched; the deep clean right after
// resets the counters anyway.
void showBootDashboard(CycleDeps &deps, const BootReport &report,
                       const StreamSnapshot &snap) {
  RenderInput in{snap, OverlayKind::Boot};
  in.boot_report = report;
  deps.renderer.render(in, deps.curr);
  deps.display.lightFull(deps.curr.data());
  CYCLE_LOG("[boot] dashboard shown for %d s (button skips)\n", report.show_s);
  deps.sleep.lightSleep(static_cast<unsigned>(report.show_s));
}

} // namespace

bool shouldPromoteToNightlyClean(unsigned next_sleep_s, time_t now,
                                 time_t last_deep_clean,
                                 const CycleConfig &cfg) {
  if (next_sleep_s <= cfg.long_sleep_for_nightly_clean_s)
    return false;
  return needsNightlyDeepClean(now, last_deep_clean,
                               cfg.nightly_deep_clean_interval_s);
}

// Post-boot tail of the cold cycle: first fetch, schedule hints, boot-check
// dashboard, first render + deep clean, sleep. Split out of runColdCycle so
// both stay under the readability-function-size thresholds.
static void finishColdCycle(CycleDeps &deps, PersistedMeta &meta,
                            BootContext ctx) {
  // First-ever render after a cold boot: deep clean for a known-good panel.
  StreamSnapshot snap;
  FetchSummary summary;
  bool ok = fetchSnapshotAndLog(deps, snap, summary);
  if (ok) {
    meta.last_api_success = deps.clock.now();
  }
  // Schedule fetch is best-effort: failure leaves `schedule.fetched_at = 0`
  // and the renderer falls back to pure realtime behaviour.
  ScheduleSnapshot schedule = deps.store.loadSchedule();
  ctx.schedule_restored = schedule.fetched_at != 0;
  if (refreshSchedule(deps, schedule)) {
    deps.store.saveSchedule(schedule);
  }

  if (deps.cfg.boot_info_show_s > 0) {
    showBootDashboard(deps, buildBootReport(deps, snap, summary, schedule, ctx),
                      snap);
  }

  CYCLE_LOG("[boot] fetch ok=%d, rendering and deep-cleaning panel\n", ok);
  time_t now = deps.clock.now();
  RenderInput in = composeRenderInput(snap, schedule, OverlayKind::None, now);
  deps.renderer.render(in, deps.curr);
  deps.display.deepClean(deps.curr.data());
  meta.last_deep_clean = now;
  meta.last_light_full = meta.last_deep_clean;
  meta.last_ntp_sync = deps.clock.lastSync();
  deps.store.saveFramebuffer(deps.curr.data(), Frame::bytes);
  meta.framebuffer_valid = true;

  SleepConfig sc = makeSleepConfig(deps.cfg);
  // planSleep wants the merged view: if realtime has nothing but hints fill
  // a slot, the next bus is the hint's time — sleep until then, not until
  // the conservative "no data" interval.
  SleepDecision sd = planSleep(in.snapshot, now, sc);
  doSleepOrLoop(deps, sd, meta);
}

void runColdCycle(CycleDeps &deps, PersistedMeta &meta) {
  CYCLE_LOG("[boot] cold path, retry %u/%u\n", meta.cold_boot_retries,
            deps.cfg.cold_boot_max_retries);
  BootContext ctx;
  ctx.meta_restored = meta.last_deep_clean != 0 || meta.last_api_success != 0;
  ctx.frame_restored = meta.framebuffer_valid;
  ctx.attempt = meta.cold_boot_retries + 1;
  BootConfig bc;
  bc.max_retries = deps.cfg.cold_boot_max_retries;
  BootResult r = runColdBoot(deps.net, deps.clock, meta.cold_boot_retries, bc);
  CYCLE_LOG("[boot] runColdBoot -> %s\n", r == BootResult::Ok ? "Ok"
                                          : r == BootResult::RetryLater
                                              ? "RetryLater"
                                              : "GiveUp");
  if (r == BootResult::RetryLater) {
    ++meta.cold_boot_retries;
    deps.store.saveMeta(meta);
    CYCLE_LOG_LN("[boot] retry later");
    deps.sleep.deepSleep(deps.cfg.cold_boot_retry_s);
    return;
  }
  if (r == BootResult::GiveUp) {
    CYCLE_LOG_LN("[boot] give up");
    RenderInput in{StreamSnapshot{}, OverlayKind::StartFailed};
    deps.renderer.render(in, deps.curr);
    deps.display.deepClean(deps.curr.data());
    meta.cold_boot_retries = 0;
    meta.framebuffer_valid = false;
    deps.store.saveMeta(meta);
    deps.sleep.deepSleep(deps.cfg.cold_boot_giveup_sleep_s);
    return;
  }
  meta.cold_boot_retries = 0;
  finishColdCycle(deps, meta, ctx);
}

// ESP32 deep sleep loses the system clock — now() returns seconds since
// boot, not Unix epoch. Without this guard, the periodic NTP check below
// (a signed subtraction) underflows to a huge negative and never fires,
// leaving planSleep to compute a 50-year deep sleep against a bogus now.
// Returns true if the clock is synced (or has just been re-synced); false
// after deep-sleeping for a retry.
bool ensureClockSynced(CycleDeps &deps, PersistedMeta &meta) {
  if (deps.clock.isSynced())
    return true;
  CYCLE_LOG("[warm] clock unsynced (now=%lld), forcing NTP\n",
            static_cast<long long>(deps.clock.now()));
  if (deps.clock.ntpSync()) {
    meta.last_ntp_sync = deps.clock.now();
    CYCLE_LOG("[warm] NTP recovered: now=%lld\n",
              static_cast<long long>(deps.clock.now()));
    return true;
  }
  CYCLE_LOG_LN("[warm] NTP failed, retrying after cold-boot interval");
  deps.sleep.deepSleep(deps.cfg.cold_boot_retry_s);
  return false;
}

void doNightlyClean(CycleDeps &deps, PersistedMeta &meta,
                    const ScheduleSnapshot &schedule,
                    const FetchCycleResult &fc, time_t now) {
  RenderInput in = composeRenderInput(fc.snap, schedule, fc.overlay, now);
  in.filter_dead_58b = fc.filter_dead_58b;
  in.oebb_auth_dead = fc.oebb_auth_dead;
  deps.renderer.render(in, deps.curr);
  deps.display.deepClean(deps.curr.data());
  meta.last_deep_clean = now;
  meta.last_light_full = now;
  meta.partial_count = 0;
  deps.store.saveFramebuffer(deps.curr.data(), Frame::bytes);
  meta.framebuffer_valid = true;
}

// Render/rescue/sleep tail of the warm cycle. Split out of runWarmCycle so
// both stay under the readability-function-size thresholds.
static void finishWarmCycle(CycleDeps &deps, PersistedMeta &meta,
                            const ScheduleSnapshot &schedule,
                            FetchCycleResult &fc, time_t now) {
  SleepConfig sc = makeSleepConfig(deps.cfg);
  SleepDecision pre = planSleep(fc.merged, now, sc);
  bool nightly = pre.mode == Mode::DeepSleep &&
                 shouldPromoteToNightlyClean(pre.seconds, now,
                                             meta.last_deep_clean, deps.cfg);

  if (nightly) {
    doNightlyClean(deps, meta, schedule, fc, now);
  } else if (fc.fetched_ok || fc.overlay != OverlayKind::None) {
    renderAndPush(deps, fc, meta, schedule);
  } else {
    // Transient fetch failure (not yet stale): keep showing the last good
    // frame. Re-rendering with an empty snap would flicker every slot to
    // "--:--" for one cycle and back.
    CYCLE_LOG_LN("[warm] fetch failed pre-stale — keeping last frame");
  }

  // Rescue: the snapshot was incomplete → keep trying after the update and
  // push one extra refresh once complete. Skipped on the nightly clean (the
  // panel just deep-cleaned into a long sleep; the next cycle re-fetches).
  if (!nightly && !fetchComplete(fc.summary)) {
    if (runRescueFetch(deps, meta, schedule, fc, deps.clock.now())) {
      // The sleep plan from the partial snapshot may be wrong in both
      // directions; recompute it from the rescued (complete) merge.
      pre = planSleep(fc.merged, deps.clock.now(), sc);
    }
  }

  doSleepOrLoop(deps, pre, meta);
}

void runWarmCycle(CycleDeps &deps, PersistedMeta &meta) {
  CYCLE_LOG_LN("[warm] cycle start");
  ScheduleSnapshot schedule = deps.store.loadSchedule();
  if (!deps.net.connect(deps.cfg.wifi_connect_ms)) {
    CYCLE_LOG_LN("[warm] wifi down");
    if (isStale(meta.last_api_success, deps.clock.now(),
                deps.cfg.stale_threshold_s)) {
      FetchCycleResult stale_fc;
      stale_fc.overlay = OverlayKind::Stale;
      renderAndPush(deps, stale_fc, meta, schedule);
    }
    deps.sleep.deepSleep(deps.cfg.poll_interval_s);
    return;
  }
  if (!ensureClockSynced(deps, meta))
    return;
  // Periodic NTP refresh.
  if (deps.clock.now() - meta.last_ntp_sync > deps.cfg.ntp_interval_s) {
    if (deps.clock.ntpSync())
      meta.last_ntp_sync = deps.clock.now();
  }

  FetchCycleResult fc = doFetchCycle(deps, meta, schedule);
  time_t now = deps.clock.now();

  // Schedule refresh: piggyback on the nightly slot (WiFi is up, we're about
  // to sleep long). Also forced if the on-RTC snapshot is stale-old.
  if (needScheduleRefresh(schedule, now)) {
    if (refreshSchedule(deps, schedule)) {
      deps.store.saveSchedule(schedule);
      // fc.merged was built from the old snapshot inside doFetchCycle, and
      // planSleep below sizes tonight's deep sleep from it. Without this,
      // refresh nights planned against day-old hints (rendering was
      // unaffected — renderAndPush re-merges internally).
      remergeAfterRefresh(fc, schedule, now);
    }
  }

  finishWarmCycle(deps, meta, schedule, fc, now);
}

void runBwReset(CycleDeps &deps, PersistedMeta &meta) {
  CYCLE_LOG_LN("[btn] BW reset + redraw");
  bool have_fb = meta.framebuffer_valid &&
                 deps.store.loadFramebuffer(deps.curr.data(), Frame::bytes) ==
                     Frame::bytes;
  if (!have_fb) {
    CYCLE_LOG_LN("[btn] no valid framebuffer — rendering empty for clean");
    StreamSnapshot snap;
    RenderInput in{snap, OverlayKind::None};
    deps.renderer.render(in, deps.curr);
  }
  deps.display.deepClean(deps.curr.data());
  if (!have_fb) {
    deps.store.saveFramebuffer(deps.curr.data(), Frame::bytes);
    meta.framebuffer_valid = true;
  }
  time_t now = deps.clock.now();
  meta.last_deep_clean = now;
  meta.last_light_full = now;
  meta.partial_count = 0;
  deps.store.saveMeta(meta);
}

void runButtonWake(CycleDeps &deps, IButton &btn, PersistedMeta &meta) {
  CYCLE_LOG_LN("[boot] button-wake");
  ButtonPress p = classifyHeld(btn, deps.cfg.btn_long_press_ms);
  if (p == ButtonPress::Long) {
    runBwReset(deps, meta);
  } else {
    CYCLE_LOG_LN("[btn] short — proceed with update");
  }
  runWarmCycle(deps, meta);
}

void pollButtonAndRunWarm(CycleDeps &deps, IButton &btn, PersistedMeta &meta) {
  constexpr std::uint32_t SETTLE_MS = 5;
  btn.init();
  btn.sleepMs(SETTLE_MS);
  if (btn.isPressed()) {
    CYCLE_LOG_LN("[btn] press detected");
    ButtonPress p = classifyHeld(btn, deps.cfg.btn_long_press_ms);
    if (p == ButtonPress::Long) {
      runBwReset(deps, meta);
    } else {
      CYCLE_LOG_LN("[btn] short — proceed with update");
    }
  }
  runWarmCycle(deps, meta);
}

} // namespace bustaferl
