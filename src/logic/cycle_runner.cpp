#include "logic/cycle_runner.h"

#include "config.h"
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
                         FetchSummary &summary, PersistedMeta &meta) {
  StreamFilter filters[STREAM_COUNT];
  buildStreamFilters(filters);
  OebbStreamFilter oebb_filter = buildOebbFilter();
  FetchInputs inputs{deps.cfg.api_base, deps.cfg.mgate_url, filters,
                     oebb_filter};
  bool ok =
      fetchSnapshot(deps.net, inputs, deps.clock.now(), out, summary, meta);
  CYCLE_LOG_STR(
      formatSnapshotSummary(out, summary.total_batches, summary.failed_batches)
          .c_str());
  return ok;
}

// Convenience overload for callers that don't need the batch summary (the
// cold path renders once and deep-cleans regardless of completeness).
bool fetchSnapshotAndLog(CycleDeps &deps, StreamSnapshot &out,
                         PersistedMeta &meta) {
  FetchSummary summary;
  return fetchSnapshotAndLog(deps, out, summary, meta);
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

void renderAndPush(CycleDeps &deps, DisplayState state,
                   const StreamSnapshot &snap, PersistedMeta &meta,
                   const ScheduleSnapshot &schedule) {
  time_t now = deps.clock.now();
  RenderInput in = composeRenderInput(state, snap, schedule, meta, now);
  deps.renderer.render(in, deps.curr);

#if UPDATE_STAMP_ENABLED
  // Reproduce the persisted frame's stamp before diffing: an unchanged
  // board then compares byte-identical and the None-skip keeps working —
  // the stamp alone never causes a panel update.
  drawUpdateStamp(deps.curr, meta.last_display_update);
#endif

  bool prev_valid = meta.framebuffer_valid;
  if (prev_valid) {
    prev_valid = deps.store.loadFramebuffer(deps.prev.data(), Frame::bytes) ==
                 Frame::bytes;
  }

  RefreshConfig rc;
  RefreshDecision d =
      planRefresh(deps.prev.data(), deps.curr.data(), prev_valid, now,
                  meta.last_light_full, meta.partial_count, rc, deps.deep_wake);

#if UPDATE_STAMP_ENABLED
  if (d.kind != RefreshKind::None) {
    // A refresh will actually reach the panel — restamp with the current
    // time and re-plan so the partial bbox covers the stamp region too.
    drawUpdateStamp(deps.curr, now);
    meta.last_display_update = now;
    d = planRefresh(deps.prev.data(), deps.curr.data(), prev_valid, now,
                    meta.last_light_full, meta.partial_count, rc,
                    deps.deep_wake);
  }
#endif

  applyDisplayDecision(deps.display, d, deps.curr.data(), meta, now);

  bool saved = deps.store.saveFramebuffer(deps.curr.data(), Frame::bytes);
  meta.framebuffer_valid = saved;
}

void doSleepOrLoop(CycleDeps &deps, const SleepDecision &sd,
                   PersistedMeta &meta, time_t now) {
  const unsigned sleep_s =
      sd.mode == Mode::DeepSleep ? sd.seconds : deps.cfg.poll_interval_s;
  // Stamp the wake target for the next cycle's drift guard. Only meaningful
  // with a real wall clock; on an unsynced now() it would poison the guard,
  // so leave it 0 (guard abstains) in that case.
  meta.expected_wake_at =
      now >= MIN_PLAUSIBLE_EPOCH ? now + static_cast<time_t>(sleep_s) : 0;
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

// Shared fetch+merge step. Hands back the realtime snapshot, the merged view
// for planSleep, plus the DisplayState the renderer should apply.
struct FetchCycleResult {
  StreamSnapshot snap;
  StreamSnapshot merged;
  FetchSummary summary;
  DisplayState state = DisplayState::Normal;
  bool fetched_ok = false;
};

// Complete ⇔ every batch (OGD + ÖBB) came back parsable. `fetched_ok` alone
// is weaker — it is true as soon as one batch survives, which is exactly the
// "some rows show --:-- although the line is running" case the rescue targets.
bool fetchComplete(const FetchSummary &s) {
  return s.total_batches > 0 && s.failed_batches == 0;
}

FetchCycleResult doFetchCycle(CycleDeps &deps, PersistedMeta &meta,
                              const ScheduleSnapshot &schedule) {
  FetchCycleResult r;
  r.fetched_ok = fetchSnapshotAndLog(deps, r.snap, r.summary, meta);
  time_t now = deps.clock.now();

  FilterHealth fh(deps.cfg.filter_health_dead_after);
  fh.setStreak(meta.filter_miss_streak);

  if (r.fetched_ok) {
    meta.last_api_success = now;
    meta.last_success_at = now;
    meta.has_any_data = true;
    // FilterHealth tracking — still wanted as a diagnostic counter even
    // though the v2 state-selector no longer renders a dedicated
    // "FilterDead" screen (subsumed by Stale/Quiet/Auth). The streak data
    // remains for filter-health monitoring on the next AID rotation.
    const bool any_service =
        std::any_of(std::begin(r.snap.stream), std::end(r.snap.stream),
                    [](const StreamData &s) { return s.slot[0].valid; });
    if (any_service) {
      fh.recordCall(r.snap.stream[STREAM_58B_ATZ].endpoint_responded,
                    r.snap.stream[STREAM_58B_ATZ].filter_matched);
      meta.filter_miss_streak = fh.streak();
    }
  }

  // Build the state-selector inputs and pick the v2 DisplayState. This
  // replaces the old "OverlayKind out of three options" computation.
  //
  // Pre-stale transient fetch failure: the selector would otherwise see an
  // empty snapshot and pick Quiet (allDeparturesBeyond({}, ...) == true).
  // That would flicker the display every cycle a single HTTP call fails.
  // Keep `state = Normal` in that window so the caller's "fc.fetched_ok ||
  // state != Normal" guard skips the redraw.
  SelectorSignals sig;
  sig.first_render_ever = !meta.has_any_data;
  sig.auth_error_seen = meta.auth_error_seen;
  sig.wifi_up = deps.net.isConnected();
  sig.now = now;
  sig.last_success = meta.last_success_at;
  if (!r.fetched_ok && (now - meta.last_success_at) <= STALE_THRESHOLD_V2_S &&
      !meta.auth_error_seen) {
    r.state = DisplayState::Normal;
  } else {
    r.state = selectDisplayState(r.snap, schedule, meta, sig);
  }

  // Stale forces an empty snapshot through the renderer (slots → "??:??").
  // The merged view for planSleep keeps the hint information so the next
  // wake still targets the right time.
  if (r.state == DisplayState::Stale) {
    r.snap = StreamSnapshot{};
  }
  r.merged = (r.state == DisplayState::Stale)
                 ? r.snap
                 : mergeSlots(r.snap, schedule, now);
  return r;
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
      renderAndPush(deps, fc.state, fc.snap, meta, schedule);
      return true;
    }
    deps.sleep.lightSleep(static_cast<unsigned>(rc.retry_pause_s));
  }
  return false;
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

// Returns true if boot succeeded (caller continues to fetch + render).
// Returns false if the cycle has already terminated (retry-sleep or give-up
// path), in which case the caller must return immediately.
bool handleColdBootOutcome(CycleDeps &deps, PersistedMeta &meta, BootResult r) {
  if (r == BootResult::RetryLater) {
    ++meta.cold_boot_retries;
    deps.store.saveMeta(meta);
    CYCLE_LOG_LN("[boot] retry later");
    deps.sleep.deepSleep(deps.cfg.cold_boot_retry_s);
    return false;
  }
  if (r == BootResult::GiveUp) {
    CYCLE_LOG_LN("[boot] give up");
    // GiveUp ends the cold-boot retry chain — render the Offline screen so
    // the user knows the device is awake but can't reach the network. v2
    // dropped the dedicated "Start fehlgeschlagen" overlay in favour of the
    // generic Offline fullscreen renderer.
    RenderInput in;
    in.state = DisplayState::Offline;
    deps.renderer.render(in, deps.curr);
    deps.display.deepClean(deps.curr.data());
    meta.cold_boot_retries = 0;
    meta.framebuffer_valid = false;
    deps.store.saveMeta(meta);
    deps.sleep.deepSleep(deps.cfg.cold_boot_giveup_sleep_s);
    return false;
  }
  meta.cold_boot_retries = 0;
  return true;
}

void runColdCycle(CycleDeps &deps, PersistedMeta &meta) {
  CYCLE_LOG("[boot] cold path, retry %u/%u\n", meta.cold_boot_retries,
            deps.cfg.cold_boot_max_retries);
  BootConfig bc;
  bc.max_retries = deps.cfg.cold_boot_max_retries;
  BootResult r = runColdBoot(deps.net, deps.clock, meta.cold_boot_retries, bc);
  CYCLE_LOG("[boot] runColdBoot -> %s\n", r == BootResult::Ok ? "Ok"
                                          : r == BootResult::RetryLater
                                              ? "RetryLater"
                                              : "GiveUp");
  if (!handleColdBootOutcome(deps, meta, r)) {
    return;
  }

  // First-ever render after a cold boot: deep clean for a known-good panel.
  StreamSnapshot snap;
  bool ok = fetchSnapshotAndLog(deps, snap, meta);
  if (ok) {
    time_t now = deps.clock.now();
    meta.last_api_success = now;
    meta.last_success_at = now;
    meta.has_any_data = true;
  }
  // Schedule fetch is best-effort: failure leaves `schedule.fetched_at = 0`
  // and the renderer falls back to pure realtime behaviour.
  ScheduleSnapshot schedule = deps.store.loadSchedule();
  if (refreshSchedule(deps, schedule)) {
    deps.store.saveSchedule(schedule);
  }
  CYCLE_LOG("[boot] fetch ok=%d, rendering and deep-cleaning panel\n", ok);
  time_t now = deps.clock.now();
  // Cold boot, post-fetch: ask the state-selector for the right screen.
  // On first-ever boot (has_any_data still false) this resolves to Boot;
  // on subsequent wakes after Update it picks Normal/Stale/etc.
  SelectorSignals sig;
  sig.first_render_ever = !meta.has_any_data;
  sig.auth_error_seen = meta.auth_error_seen;
  sig.wifi_up = deps.net.isConnected();
  sig.now = now;
  sig.last_success = meta.last_success_at;
  DisplayState state = selectDisplayState(snap, schedule, meta, sig);
  RenderInput in = composeRenderInput(state, snap, schedule, meta, now);
  deps.renderer.render(in, deps.curr);
#if UPDATE_STAMP_ENABLED
  drawUpdateStamp(deps.curr, now);
  meta.last_display_update = now;
#endif
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
  doSleepOrLoop(deps, sd, meta, now);
}

// True when the wall clock reads implausibly far past the epoch this device
// asked to wake at. isSynced() only enforces a LOWER bound (> 2023), so a
// corrupt RTC that woke up hours ahead — but still past 2023 — passes it.
// This is the field-observed "58B coma": schedule-hint departures rendered
// against a now() that was hours off, showing wrong times. The previous cycle
// persisted expected_wake_at = now + planned_sleep; a healthy clock lands at
// ~that value, so anything past it by more than max_wake_overshoot_s is a
// corrupt clock. Needs a trustworthy reference: expected_wake_at == 0 means we
// have never slept with a known wall clock (first boot), so we abstain and let
// the lower-bound / periodic paths run.
bool clockDriftedAhead(IClock &clock, const PersistedMeta &meta,
                       const CycleConfig &cfg) {
  if (meta.expected_wake_at < MIN_PLAUSIBLE_EPOCH)
    return false;
  return clock.now() > meta.expected_wake_at + cfg.max_wake_overshoot_s;
}

// ESP32 deep sleep loses the system clock — now() returns seconds since
// boot, not Unix epoch. Without this guard, the periodic NTP check below
// (a signed subtraction) underflows to a huge negative and never fires,
// leaving planSleep to compute a 50-year deep sleep against a bogus now.
// The second guard (clockDriftedAhead) catches the opposite corruption: a
// clock that came back plausible-looking but hours off. Both resolve the
// same way — force an NTP re-sync, and only proceed on the corrected clock.
// Returns true if the clock is synced (or has just been re-synced); false
// after deep-sleeping for a retry.
bool ensureClockSynced(CycleDeps &deps, PersistedMeta &meta) {
  const bool unsynced = !deps.clock.isSynced();
  const bool drifted = clockDriftedAhead(deps.clock, meta, deps.cfg);
  if (!unsynced && !drifted)
    return true;
  CYCLE_LOG("[warm] clock %s (now=%lld), forcing NTP\n",
            unsynced ? "unsynced" : "drifted-ahead",
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
  RenderInput in = composeRenderInput(fc.state, fc.snap, schedule, meta, now);
  deps.renderer.render(in, deps.curr);
#if UPDATE_STAMP_ENABLED
  drawUpdateStamp(deps.curr, now);
  meta.last_display_update = now;
#endif
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
  } else if (fc.fetched_ok || fc.state != DisplayState::Normal) {
    renderAndPush(deps, fc.state, fc.snap, meta, schedule);
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
      now = deps.clock.now();
      pre = planSleep(fc.merged, now, sc);
    }
  }

  doSleepOrLoop(deps, pre, meta, now);
}

void runWarmCycle(CycleDeps &deps, PersistedMeta &meta) {
  CYCLE_LOG_LN("[warm] cycle start");
  ScheduleSnapshot schedule = deps.store.loadSchedule();
  if (!deps.net.connect(deps.cfg.wifi_connect_ms)) {
    CYCLE_LOG_LN("[warm] wifi down");
    // No WiFi → ask the selector whether to surface Offline / Stale / keep
    // the last frame. SelectorSignals.wifi_up=false drives the choice.
    time_t now = deps.clock.now();
    SelectorSignals sig;
    sig.first_render_ever = !meta.has_any_data;
    sig.auth_error_seen = meta.auth_error_seen;
    sig.wifi_up = false;
    sig.now = now;
    sig.last_success = meta.last_success_at;
    DisplayState s = selectDisplayState(StreamSnapshot{}, schedule, meta, sig);
    if (s == DisplayState::Stale || s == DisplayState::Offline) {
      renderAndPush(deps, s, StreamSnapshot{}, meta, schedule);
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
    RenderInput in;
    in.state = DisplayState::Normal;
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
