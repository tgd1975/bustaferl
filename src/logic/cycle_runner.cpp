#include "logic/cycle_runner.h"

#include "config.h"
#include "data/DiagView.h"
#include "data/StreamSnapshot.h"
#include "logic/boot_sequencer.h"
#include "logic/button_classifier.h"
#include "logic/cycle_trace.h"
#include "logic/diag_mode.h"
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
#include "logic/ssid_match.h"
#include "render/diag_page.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
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

// Fill the Offline-screen diagnostic fields from the live network: the visible
// scan, the configured SSIDs, and the case-only mismatch hint (e.g. configured
// "A-NET2" vs broadcast "a-net2"). Shared by both Offline render sites (cold
// give-up and warm wifi-down).
void fillOfflineDiagnostics(CycleDeps &deps, RenderInput &in) {
  in.visible_aps = deps.net.scanVisible();
  in.wanted_ssids = deps.net.configuredSsids();
  in.case_mismatch = findCaseMismatch(in.wanted_ssids, in.visible_aps);
}

// Paint the "KEIN EMPFANG" screen with a fresh scan. `first_paint` picks a
// deep-clean (crisp baseline the first time it appears) vs a single-flash
// light-full for the once-a-minute refreshes while WiFi stays down — the cold
// retry loop re-scans and repaints every cycle so the found-SSID list stays
// current without hammering the panel with 3× deep-clean flashes each minute.
void paintNoNetworkScreen(CycleDeps &deps, bool first_paint) {
  RenderInput in;
  in.state = DisplayState::Offline;
  fillOfflineDiagnostics(deps, in);
  deps.renderer.render(in, deps.curr);
  if (first_paint) {
    deps.display.deepClean(deps.curr.data());
  } else {
    deps.display.lightFull(deps.curr.data());
  }
}

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

// `force_stamp` (button-triggered cycle): stamp `now` before the diff so the
// changed stamp region alone forces at least a partial update. This gives the
// user visible feedback ("upd HH:MM" advances) on every press, even when the
// departure data is byte-identical to what's already on the glass.
void renderAndPush(CycleDeps &deps, DisplayState state,
                   const StreamSnapshot &snap, PersistedMeta &meta,
                   const ScheduleSnapshot &schedule, bool force_stamp = false) {
  time_t now = deps.clock.now();
  RenderInput in = composeRenderInput(state, snap, schedule, meta, now);
  // composeRenderInput is a pure function with no network access; the Offline
  // screen's diagnostic block (visible scan, configured SSIDs, case-mismatch
  // hint) needs the live network, so fill it here.
  if (state == DisplayState::Offline) {
    fillOfflineDiagnostics(deps, in);
  }
  deps.renderer.render(in, deps.curr);

#if UPDATE_STAMP_ENABLED
  // Normally reproduce the persisted frame's stamp before diffing: an unchanged
  // board then compares byte-identical and the None-skip keeps working — the
  // stamp alone never causes a panel update. When forced (button press), stamp
  // `now` instead so the diff is non-empty and a refresh reaches the panel.
  drawUpdateStamp(deps.curr, force_stamp ? now : meta.last_display_update);
  if (force_stamp) {
    meta.last_display_update = now;
  }
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
  if (!force_stamp && d.kind != RefreshKind::None) {
    // A refresh will actually reach the panel — restamp with the current
    // time and re-plan so the partial bbox covers the stamp region too.
    // (The forced path already stamped `now` above, before diffing.)
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
  CYCLE_LOG("[sleep] staying active, waiting %u s\n", deps.cfg.poll_interval_s);
  deps.sleep.pause(deps.cfg.poll_interval_s);
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
    // though the state-selector renders no dedicated "FilterDead" screen. The
    // streak data remains for filter-health monitoring on the next AID
    // rotation.
    const bool any_service =
        std::any_of(std::begin(r.snap.stream), std::end(r.snap.stream),
                    [](const StreamData &s) { return s.slot[0].valid; });
    if (any_service) {
      fh.recordCall(r.snap.stream[STREAM_58B_ATZ].endpoint_responded,
                    r.snap.stream[STREAM_58B_ATZ].filter_matched);
      meta.filter_miss_streak = fh.streak();
    }
  }

  // Pick the display state. Only the error/placeholder screens are states now
  // (Auth / Boot / Offline); anything with data is the Normal board.
  SelectorSignals sig;
  sig.first_render_ever = !meta.has_any_data;
  sig.auth_error_seen = meta.auth_error_seen;
  sig.wifi_up = deps.net.isConnected();
  sig.now = now;
  sig.last_success = meta.last_success_at;
  r.state = selectDisplayState(r.snap, schedule, meta, sig);

  // Merge realtime with the schedule hints for both the render and planSleep.
  // On a fetch failure the realtime snapshot is empty, so the merge falls back
  // to the scheduled departures — the board keeps showing the next departure
  // (its real time) instead of blanking to "--:--".
  r.merged = mergeSlots(r.snap, schedule, now);
  return r;
}

// Incomplete-snapshot rescue: the cycle already rendered what it had; keep
// re-fetching inside the [window_start, window_end] window after that display
// update and push one extra refresh as soon as a complete snapshot arrives.
// The window's lower bound keeps two panel updates from landing back-to-back.
// The button is never polled here — a press during a rescue merely ends the
// pacing pause() early; the running update finishes, it is not interrupted.
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
      deps.sleep.pause(wait_s);
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
    deps.sleep.pause(static_cast<unsigned>(rc.retry_pause_s));
  }
  return false;
}

// --- Diagnostic trace stamping (data/CycleTrace.h) ---------------------
// One CycleRecord per warm cycle + anomaly ErrorRecords, so the double-click
// diagnostic screens can reconstruct "what happened" without a serial log.

std::uint32_t traceEpoch(time_t now) {
  return now >= MIN_PLAUSIBLE_EPOCH ? static_cast<std::uint32_t>(now) : 0U;
}

std::uint16_t streamOkFlags(const StreamSnapshot &snap) {
  std::uint16_t f = 0;
  if (snap.stream[STREAM_58A_ATZ].slot[0].valid)
    f |= CYC_STREAM0_OK;
  if (snap.stream[STREAM_58A_HIETZING].slot[0].valid)
    f |= CYC_STREAM1_OK;
  if (snap.stream[STREAM_58B_ATZ].slot[0].valid)
    f |= CYC_STREAM2_OK;
  if (snap.stream[STREAM_SBAHN_HBF].slot[0].valid)
    f |= CYC_STREAM3_OK;
  return f;
}

// Outcome bits the fetch/render/sleep code gathers for the trace record.
// Bundled (not loose params) so traceCycle stays under the parameter
// threshold.
struct CycleOutcome {
  CycleTrigger trigger = CycleTrigger::Timer;
  bool rendered = false;
  bool rescue_tried = false;
  bool rescue_ok = false;
  bool wifi_failed = false;
};

void pushError(CycleTrace &t, TraceError code, time_t now) {
  tracePushError(
      t, ErrorRecord{traceEpoch(now), static_cast<std::uint8_t>(code), 0});
}

void traceCycle(CycleDeps &deps, const FetchCycleResult &fc,
                const SleepDecision &sd, const CycleOutcome &oc, time_t now) {
  CycleTrace t = deps.store.loadTrace();

  // Anomaly: WiFi failure this cycle. (The Stale enter/exit transitions were
  // dropped along with the Stale display state — old data now just renders as
  // the schedule-backed Normal board, so there is no state edge to trace.)
  if (oc.wifi_failed)
    pushError(t, TraceError::WifiFail, now);

  CycleRecord rec;
  rec.at = traceEpoch(now);
  rec.flags = streamOkFlags(fc.snap);
  if (oc.rendered)
    rec.flags |= CYC_RENDERED;
  if (oc.rescue_tried)
    rec.flags |= CYC_RESCUE_TRIED;
  if (oc.rescue_ok)
    rec.flags |= CYC_RESCUE_OK;
  if (sd.mode == Mode::DeepSleep)
    rec.flags |= CYC_DEEP_SLEEP;
  const unsigned sleep_s =
      sd.mode == Mode::DeepSleep ? sd.seconds : deps.cfg.poll_interval_s;
  rec.sleep_s = static_cast<std::uint16_t>(
      std::min<unsigned>(sleep_s, std::numeric_limits<std::uint16_t>::max()));
  rec.trigger = static_cast<std::uint8_t>(oc.trigger);
  rec.failed_batches = static_cast<std::uint8_t>(fc.summary.failed_batches);
  rec.retried_batches = static_cast<std::uint8_t>(fc.summary.retried_batches);
  tracePushCycle(t, rec);

  deps.store.saveTrace(t);
}

// --- Diagnostic view assembly (data/DiagView.h) ------------------------

// Live heap + uptime probe. Device-only (ESP APIs); a no-op on the host so
// the STATUS page's numbers simply read 0 in native tests.
#ifndef NATIVE_BUILD
void probeSystem(DiagView &v) {
  constexpr std::uint32_t BYTES_PER_KB = 1024;
  constexpr std::uint32_t MS_PER_S = 1000;
  v.heap_free_kb = ESP.getFreeHeap() / BYTES_PER_KB;
  v.heap_largest_kb = ESP.getMaxAllocHeap() / BYTES_PER_KB;
  v.uptime_s = millis() / MS_PER_S;
}
#else
void probeSystem(DiagView & /*v*/) {}
#endif

// Fill the fields shared by the diagnostic pages and the boot-check screen from
// the live HAL + persisted meta + the passed snapshot/schedule. Boot-specific
// extras (attempt count, batch stats, RTC-restore flags) are layered on top by
// buildBootCheckView.
DiagView buildDiagView(CycleDeps &deps, const PersistedMeta &meta,
                       const StreamSnapshot &snap,
                       const ScheduleSnapshot &schedule) {
  DiagView v;
  const NetInfo ni = deps.net.connectionInfo();
  v.has_net_info = ni.valid;
  std::snprintf(v.ssid, sizeof(v.ssid), "%s", ni.ssid);
  std::snprintf(v.ip, sizeof(v.ip), "%s", ni.ip);
  v.rssi_dbm = ni.rssi_dbm;
  v.now = deps.clock.now();
  v.ntp_ok = deps.clock.isSynced();
  v.last_ntp_sync = meta.last_ntp_sync;
  v.snap = snap;
  v.schedule = schedule;
  v.filter_miss_streak = meta.filter_miss_streak;
  v.ogd_auth_streak = meta.ogd_auth_streak;
  v.auth_error_seen = meta.auth_error_seen;
  v.partial_count = meta.partial_count;
  v.last_light_full = meta.last_light_full;
  v.last_deep_clean = meta.last_deep_clean;
  v.last_reset_reason = meta.last_reset_reason;
  probeSystem(v);
  v.trace = deps.store.loadTrace();
  return v;
}

// What the boot-check screen adds on top of the live status: which cold-boot
// attempt succeeded, whether the RTC slots survived, and the fetch batch tally.
struct ColdBootStats {
  int attempt = 1;
  int attempts_max = 1;
  bool meta_restored = false;
  bool frame_restored = false;
  bool schedule_restored = false;
};

DiagView buildBootCheckView(CycleDeps &deps, const PersistedMeta &meta,
                            const StreamSnapshot &snap,
                            const ScheduleSnapshot &schedule,
                            const FetchSummary &summary,
                            const ColdBootStats &stats) {
  DiagView v = buildDiagView(deps, meta, snap, schedule);
  v.boot_attempt = stats.attempt;
  v.boot_attempts_max = stats.attempts_max;
  v.meta_restored = stats.meta_restored;
  v.frame_restored = stats.frame_restored;
  v.schedule_restored = stats.schedule_restored;
  v.batches_total = summary.total_batches;
  v.batches_failed = summary.failed_batches;
  v.batches_retried = summary.retried_batches;
  v.show_s = deps.cfg.boot_info_show_s;
  return v;
}

// Records the cold boot as a ColdBoot cycle so it shows up on the diagnostic
// CYCLES page alongside the warm cycles.
void traceColdBoot(CycleDeps &deps, const StreamSnapshot &snap,
                   const FetchSummary &summary, DisplayState state,
                   const SleepDecision &sd, time_t now) {
  FetchCycleResult fc;
  fc.snap = snap;
  fc.summary = summary;
  fc.state = state;
  CycleOutcome oc;
  oc.trigger = CycleTrigger::ColdBoot;
  oc.rendered = true;
  traceCycle(deps, fc, sd, oc, now);
}

// Boot-check dashboard: render + deep-clean it and hold for boot_info_show_s
// (a boot-button tap ends the wait early to skip ahead to the board).
void showBootCheck(CycleDeps &deps, const PersistedMeta &meta,
                   const StreamSnapshot &snap, const ScheduleSnapshot &schedule,
                   const FetchSummary &summary, const ColdBootStats &stats) {
  if (deps.cfg.boot_info_show_s <= 0)
    return;
  DiagView v = buildBootCheckView(deps, meta, snap, schedule, summary, stats);
  renderBootCheck(v, deps.curr);
  deps.display.deepClean(deps.curr.data());
  deps.sleep.pause(static_cast<unsigned>(deps.cfg.boot_info_show_s));
}

// Render/clean/sleep tail of the cold cycle. Split from runColdCycle to keep
// both under the readability-function-size thresholds.
void finishColdCycle(CycleDeps &deps, PersistedMeta &meta,
                     const StreamSnapshot &snap,
                     const ScheduleSnapshot &schedule,
                     const FetchSummary &summary) {
  time_t now = deps.clock.now();
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
  // If the boot-check ran it already deep-cleaned the panel, so a single-flash
  // light full clears its ghost; otherwise deep-clean here for the first
  // known-good frame.
  if (deps.cfg.boot_info_show_s > 0) {
    deps.display.lightFull(deps.curr.data());
  } else {
    deps.display.deepClean(deps.curr.data());
  }
  meta.last_deep_clean = now;
  meta.last_light_full = now;
  meta.last_ntp_sync = deps.clock.lastSync();
  deps.store.saveFramebuffer(deps.curr.data(), Frame::bytes);
  meta.framebuffer_valid = true;

  SleepConfig sc = makeSleepConfig(deps.cfg);
  // planSleep wants the merged view: if realtime has nothing but hints fill a
  // slot, the next bus is the hint's time — sleep until then, not until the
  // conservative "no data" interval.
  SleepDecision sd = planSleep(in.snapshot, now, sc);
  traceColdBoot(deps, snap, summary, state, sd, now);
  doSleepOrLoop(deps, sd, meta, now);
}

} // namespace

CycleKind selectCycle(WakeCause cause, uint8_t cold_boot_retries,
                      bool has_any_data) {
  if (cause == WakeCause::Button) {
    return CycleKind::Button;
  }
  // The cold (boot-screen) path is only for a device with nothing to show yet:
  //   - genuinely never fetched (!has_any_data), or
  //   - still inside the cold retry loop (cold_boot_retries > 0), which exits
  //     via deepSleep() and so wakes back as a Timer, not ColdBoot.
  // Once the device has a board this returns Warm regardless of wake cause.
  // That is deliberate for the ColdBoot cause specifically: ColdBoot means
  // "reset, not a deep-sleep wake". A real power-on wipes RTC memory, so
  // has_any_data is false and this is a true cold boot. But a brownout (WiFi-
  // current spike), watchdog, panic or software reset during warm operation is
  // ALSO reported as ColdBoot while leaving RTC — and thus has_any_data —
  // intact. Routing that to the cold path is what re-flashed the boot screen
  // mid-operation; gating on has_any_data sends it to a warm cycle that just
  // reconnects and repaints the real board, no boot screen.
  const bool needs_cold = cold_boot_retries > 0 || !has_any_data;
  return needs_cold ? CycleKind::Cold : CycleKind::Warm;
}

bool shouldPromoteToNightlyClean(unsigned next_sleep_s, time_t now,
                                 time_t last_deep_clean,
                                 const CycleConfig &cfg) {
  if (next_sleep_s <= cfg.long_sleep_for_nightly_clean_s)
    return false;
  return needsNightlyDeepClean(now, last_deep_clean,
                               cfg.nightly_deep_clean_interval_s);
}

// Returns true if boot succeeded (caller continues to fetch + render).
// Returns false if WiFi/NTP failed — the cycle has (possibly) painted "KEIN
// EMPFANG" and scheduled a 60 s retry-sleep, so the caller must return
// immediately.
//
// There is no permanent "give up": as long as the device has never connected it
// stays on the cold path (see setup()'s routing on !has_any_data). Every failed
// attempt retries WiFi after 60 s, but the panel is repainted only every
// `no_wifi_repaint_every`th cycle (≈5 min) — e-paper full refreshes are slow
// and the scan rarely changes minute-to-minute, so there is no point flashing
// it every retry. cold_boot_retries still climbs (capped) for the boot-check
// attempt display; no_wifi_cycles free-runs to gate the repaint.
bool handleColdBootOutcome(CycleDeps &deps, PersistedMeta &meta, BootResult r) {
  if (r == BootResult::Ok) {
    meta.cold_boot_retries = 0;
    meta.no_wifi_cycles = 0;
    return true;
  }
  // Wrong WiFi password: the AP accepted the association but rejected the WPA
  // handshake. Retrying with the same credentials can never work, so this is
  // terminal — paint the dedicated screen naming the SSID and deep-sleep a long
  // interval (we still wake occasionally in case the AP/password changed)
  // rather than spinning the 60 s no-network loop.
  if (deps.net.lastFailure() == WifiFailure::AuthFailed) {
    CYCLE_LOG_LN("[boot] wrong wifi password — terminal");
    RenderInput in;
    in.state = DisplayState::WifiAuth;
    in.wanted_ssids = deps.net.configuredSsids();
    deps.renderer.render(in, deps.curr);
    deps.display.deepClean(deps.curr.data());
    meta.cold_boot_retries = 0;
    meta.no_wifi_cycles = 0;
    meta.framebuffer_valid = false;
    deps.store.saveMeta(meta);
    deps.sleep.deepSleep(deps.cfg.wifi_auth_sleep_s);
    return false;
  }
  // Repaint only on the 0th, Nth, 2Nth … no-wifi cycle. The first appearance
  // (no_wifi_cycles == 0, boot screen still showing) gets a crisp deep-clean;
  // the periodic repaints after that use a light single-flash. Retry (sleep)
  // happens every cycle regardless.
  const unsigned every =
      deps.cfg.no_wifi_repaint_every > 0 ? deps.cfg.no_wifi_repaint_every : 1;
  const bool first_paint = meta.no_wifi_cycles == 0;
  const bool repaint = (meta.no_wifi_cycles % every) == 0;
  CYCLE_LOG("[boot] no wifi (cycle %u, %s)\n", meta.no_wifi_cycles,
            repaint ? "repaint" : "skip");
  if (repaint) {
    paintNoNetworkScreen(deps, first_paint);
  }
  // cold_boot_retries caps at max (attempt display); no_wifi_cycles free-runs
  // (uint8 wrap is harmless — it just lands back on a repaint boundary).
  if (meta.cold_boot_retries < deps.cfg.cold_boot_max_retries) {
    ++meta.cold_boot_retries;
  }
  ++meta.no_wifi_cycles;
  meta.framebuffer_valid = false;
  deps.store.saveMeta(meta);
  deps.sleep.deepSleep(deps.cfg.cold_boot_retry_s);
  return false;
}

// First thing a genuine cold boot does: put the "loading…" board on the glass
// so the user sees the device is alive while WiFi + NTP + the first fetch run.
// Suppressed when either (a) a trustworthy frame is already on the panel, or
// (b) we are on a 60 s cold-boot *retry* (cold_boot_retries > 0): the retry
// re-enters runColdCycle but the boot screen is already showing, so re-flashing
// the byte-identical screen every retry would only wear the panel. A single
// lightFull is enough — finishColdCycle deep-cleans the real board afterwards.
void showBootScreen(CycleDeps &deps, const PersistedMeta &meta) {
  if (meta.framebuffer_valid || meta.cold_boot_retries > 0) {
    return;
  }
  CYCLE_LOG_LN("[boot] show boot screen");
  RenderInput in;
  in.state = DisplayState::Boot;
  in.firmware_version = DISPLAY_VERSION_STR;
  deps.renderer.render(in, deps.curr);
  deps.display.lightFull(deps.curr.data());
}

namespace {
const char *resetReasonText(ResetReason reason) {
  switch (reason) {
  case ResetReason::Brownout:
    return "BROWNOUT";
  case ResetReason::WatchdogOrPanic:
    return "WATCHDOG/PANIC";
  case ResetReason::Other:
    return "RESET";
  case ResetReason::Normal:
  default:
    return "";
  }
}
} // namespace

void showBrownoutScreen(CycleDeps &deps, ResetReason reason) {
  if (reason == ResetReason::Normal || deps.cfg.brownout_show_s <= 0) {
    return;
  }
  CYCLE_LOG("[boot] unplanned reset, reason=%d\n", static_cast<int>(reason));
  renderBrownoutScreen(resetReasonText(reason), deps.curr);
  deps.display.deepClean(deps.curr.data());
  deps.sleep.pause(static_cast<unsigned>(deps.cfg.brownout_show_s));
}

void runColdCycle(CycleDeps &deps, PersistedMeta &meta) {
  CYCLE_LOG("[boot] cold path, retry %u/%u\n", meta.cold_boot_retries,
            deps.cfg.cold_boot_max_retries);
  // Boot screen before the network work (CONCEPT.md §8): visible feedback
  // first, boot process second.
  showBootScreen(deps, meta);
  // Snapshot the boot context for the boot-check screen before the outcome
  // handler resets cold_boot_retries / the render overwrites framebuffer_valid.
  ColdBootStats stats;
  stats.attempt = static_cast<int>(meta.cold_boot_retries) + 1;
  stats.attempts_max = static_cast<int>(deps.cfg.cold_boot_max_retries);
  stats.meta_restored = meta.has_any_data;
  stats.frame_restored = meta.framebuffer_valid;

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

  StreamSnapshot snap;
  FetchSummary summary;
  bool ok = fetchSnapshotAndLog(deps, snap, summary, meta);
  if (ok) {
    time_t now = deps.clock.now();
    meta.last_api_success = now;
    meta.last_success_at = now;
    meta.has_any_data = true;
  }
  // Schedule fetch is best-effort: failure leaves `schedule.fetched_at = 0`
  // and the renderer falls back to pure realtime behaviour.
  ScheduleSnapshot schedule = deps.store.loadSchedule();
  stats.schedule_restored = schedule.fetched_at != 0;
  if (refreshSchedule(deps, schedule)) {
    deps.store.saveSchedule(schedule);
  }
  CYCLE_LOG("[boot] fetch ok=%d\n", ok);

  // Boot-check dashboard first (system self-test the user can read), then the
  // departure board.
  showBootCheck(deps, meta, snap, schedule, summary, stats);
  finishColdCycle(deps, meta, snap, schedule, summary);
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
                            FetchCycleResult &fc, time_t now,
                            CycleTrigger trigger) {
  // A button-triggered cycle forces the update stamp (visible feedback even on
  // unchanged data); timer cycles do not. force_stamp is fully determined by
  // the trigger, so it is derived here rather than passed as a 7th parameter.
  const bool force_stamp = trigger == CycleTrigger::Button;
  SleepConfig sc = makeSleepConfig(deps.cfg);
  SleepDecision pre = planSleep(fc.merged, now, sc);
  bool nightly = pre.mode == Mode::DeepSleep &&
                 shouldPromoteToNightlyClean(pre.seconds, now,
                                             meta.last_deep_clean, deps.cfg);

  bool rendered = false;
  if (nightly) {
    doNightlyClean(deps, meta, schedule, fc, now);
    rendered = true;
  } else if (fc.fetched_ok || fc.state != DisplayState::Normal || force_stamp) {
    // force_stamp (button press) pushes even on a fetch failure: the user
    // pressed the button, so give visible feedback (stamp advances) rather than
    // silently keeping the last frame.
    renderAndPush(deps, fc.state, fc.snap, meta, schedule, force_stamp);
    rendered = true;
  } else {
    // Timer-triggered fetch failure: keep showing the last good frame rather
    // than re-rendering. (The schedule-backed merge would still have valid
    // times, but freezing avoids churning the panel on every failed poll.)
    CYCLE_LOG_LN("[warm] fetch failed — keeping last frame");
  }

  // Rescue: the snapshot was incomplete → keep trying after the update and
  // push one extra refresh once complete. Skipped on the nightly clean (the
  // panel just deep-cleaned into a long sleep; the next cycle re-fetches).
  const bool rescue_tried = !nightly && !fetchComplete(fc.summary);
  bool rescue_ok = false;
  if (rescue_tried) {
    rescue_ok = runRescueFetch(deps, meta, schedule, fc, deps.clock.now());
    if (rescue_ok) {
      // The sleep plan from the partial snapshot may be wrong in both
      // directions; recompute it from the rescued (complete) merge.
      now = deps.clock.now();
      pre = planSleep(fc.merged, now, sc);
    }
  }

  CycleOutcome oc;
  oc.trigger = trigger;
  oc.rendered = rendered || rescue_ok;
  oc.rescue_tried = rescue_tried;
  oc.rescue_ok = rescue_ok;
  traceCycle(deps, fc, pre, oc, now);

  doSleepOrLoop(deps, pre, meta, now);
}

// Warm-cycle WiFi-down branch. Split out of runWarmCycle to keep it under the
// readability-function-size threshold. Always terminates the cycle (sleeps),
// so the caller returns right after.
void handleWarmWifiDown(CycleDeps &deps, PersistedMeta &meta,
                        const ScheduleSnapshot &schedule, CycleTrigger trigger,
                        bool force_stamp) {
  CYCLE_LOG_LN("[warm] wifi down");
  // Wrong password mid-life (router credentials changed) is terminal here too —
  // show the dedicated screen and long-sleep instead of the 30 s poll loop.
  if (deps.net.lastFailure() == WifiFailure::AuthFailed) {
    CYCLE_LOG_LN("[warm] wrong wifi password — terminal");
    RenderInput in;
    in.state = DisplayState::WifiAuth;
    in.wanted_ssids = deps.net.configuredSsids();
    deps.renderer.render(in, deps.curr);
    deps.display.deepClean(deps.curr.data());
    meta.framebuffer_valid = false;
    deps.store.saveMeta(meta);
    deps.sleep.deepSleep(deps.cfg.wifi_auth_sleep_s);
    return;
  }
  // No WiFi → surface the Offline "KEIN EMPFANG" screen once the data is old
  // enough (OFFLINE_THRESHOLD_S); until then keep the last good frame rather
  // than blanking. SelectorSignals.wifi_up=false drives the choice: the
  // selector returns Offline past the threshold, else Normal.
  time_t now = deps.clock.now();
  SelectorSignals sig;
  sig.first_render_ever = !meta.has_any_data;
  sig.auth_error_seen = meta.auth_error_seen;
  sig.wifi_up = false;
  sig.now = now;
  sig.last_success = meta.last_success_at;
  DisplayState s = selectDisplayState(StreamSnapshot{}, schedule, meta, sig);
  bool rendered = false;
  if (s == DisplayState::Offline) {
    renderAndPush(deps, s, StreamSnapshot{}, meta, schedule, force_stamp);
    rendered = true;
  }
  FetchCycleResult wfc;
  wfc.state = s;
  CycleOutcome oc;
  oc.trigger = trigger;
  oc.rendered = rendered;
  oc.wifi_failed = true;
  traceCycle(deps, wfc,
             SleepDecision{Mode::DeepSleep, deps.cfg.poll_interval_s}, oc, now);
  deps.sleep.deepSleep(deps.cfg.poll_interval_s);
}

void runWarmCycle(CycleDeps &deps, PersistedMeta &meta, CycleTrigger trigger) {
  CYCLE_LOG_LN("[warm] cycle start");
  const bool force_stamp = trigger == CycleTrigger::Button;
  ScheduleSnapshot schedule = deps.store.loadSchedule();
  if (!deps.net.connect(deps.cfg.wifi_connect_ms)) {
    handleWarmWifiDown(deps, meta, schedule, trigger, force_stamp);
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

  finishWarmCycle(deps, meta, schedule, fc, now, trigger);
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

void runDiagMode(CycleDeps &deps, IButton &btn, PersistedMeta &meta) {
  CYCLE_LOG_LN("[diag] enter");
  // Poll cadence while the page is on screen — tight enough to catch a tap,
  // idle enough not to spin. The wall clock (RTC-backed) drives the timeout.
  constexpr std::uint32_t DIAG_WAIT_POLL_MS = 50;
  btn.init();

  // Best-effort live fetch so STATUS / DATA reflect the current network —
  // an empty snapshot is itself a useful diagnostic.
  ScheduleSnapshot schedule = deps.store.loadSchedule();
  StreamSnapshot snap;
  if (deps.net.connect(deps.cfg.wifi_connect_ms)) {
    snap = doFetchCycle(deps, meta, schedule).snap;
  }
  DiagView v = buildDiagView(deps, meta, snap, schedule);

  int page = static_cast<int>(DiagPage::Status);
  const time_t start = deps.clock.now();
  for (;;) {
    v.diag_page = page;
    renderDiagPage(v, static_cast<DiagPage>(page), deps.curr);
    deps.display.lightFull(deps.curr.data());

    ButtonPress press = ButtonPress::None;
    bool timed_out = false;
    while (press == ButtonPress::None && !timed_out) {
      if (btn.isPressed()) {
        press = classifyHeld(btn, deps.cfg.btn_long_press_ms);
      } else if (deps.clock.now() - start >= deps.cfg.diag_max_s) {
        timed_out = true;
      } else {
        btn.sleepMs(DIAG_WAIT_POLL_MS);
      }
    }
    DiagStep step = diagNext(page, press, timed_out);
    if (step.action == DiagAction::Exit) {
      CYCLE_LOG_LN("[diag] exit");
      return;
    }
    page = step.page;
  }
}

void runButtonWake(CycleDeps &deps, IButton &btn, PersistedMeta &meta) {
  CYCLE_LOG_LN("[boot] button-wake");
  ButtonPress p = classifyPress(btn, deps.cfg.btn_long_press_ms,
                                deps.cfg.btn_double_click_ms);
  if (p == ButtonPress::Long) {
    // classifyHeld returned at the 3 s mark while the button is still down.
    // Drain the hold before the reset re-arms EXT0/GPIO wake so a finger still
    // on the button doesn't immediately re-wake the next sleep.
    waitForRelease(btn);
    runBwReset(deps, meta);
  } else if (p == ButtonPress::Double) {
    runDiagMode(deps, btn, meta);
  } else {
    CYCLE_LOG_LN("[btn] short — proceed with update");
  }
  // Always button-triggered here → the Button trigger forces the update stamp
  // so the press gives visible feedback even when the departure data is
  // unchanged. After a diagnostic session this also re-renders the board.
  runWarmCycle(deps, meta, CycleTrigger::Button);
}

void pollButtonAndRunWarm(CycleDeps &deps, IButton &btn, PersistedMeta &meta) {
  constexpr std::uint32_t SETTLE_MS = 5;
  btn.init();
  btn.sleepMs(SETTLE_MS);
  bool pressed = btn.isPressed();
  if (pressed) {
    CYCLE_LOG_LN("[btn] press detected");
    ButtonPress p = classifyPress(btn, deps.cfg.btn_long_press_ms,
                                  deps.cfg.btn_double_click_ms);
    if (p == ButtonPress::Long) {
      // Long fires at the timeout while still held — drain the hold before the
      // reset, else the still-LOW line ends the warm cycle's next pause()
      // immediately and we get a second reset.
      waitForRelease(btn);
      runBwReset(deps, meta);
    } else if (p == ButtonPress::Double) {
      runDiagMode(deps, btn, meta);
    } else {
      CYCLE_LOG_LN("[btn] short — proceed with update");
    }
  }
  // A Button trigger forces the stamp; the routine poll-timer wake stays a
  // Timer so idle polls still no-op on unchanged data.
  runWarmCycle(deps, meta,
               pressed ? CycleTrigger::Button : CycleTrigger::Timer);
}

} // namespace bustaferl
