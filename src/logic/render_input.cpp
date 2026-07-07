#include "logic/render_input.h"

#include "config.h"
#include "logic/slot_merger.h"

#include <algorithm>
#include <cstring>

namespace bustaferl {

bool allDeparturesBeyond(const StreamSnapshot &snap, std::time_t horizon) {
  // cppcheck-suppress useStlAlgorithm
  for (const StreamData &stream : snap.stream) {
    // cppcheck-suppress useStlAlgorithm
    for (const Departure &d : stream.slot) {
      if (d.valid && d.when <= horizon) {
        return false;
      }
    }
  }
  return true;
}

bool outsideServiceWindow(std::time_t now) {
  struct tm local{};
  localtime_r(&now, &local);
  const int h = local.tm_hour;
  // END < START → night wraps midnight. Default 5/1 means service runs
  // 05:00 → 00:59 and night is [01:00, 05:00).
  if (SERVICE_WINDOW_END_HOUR < SERVICE_WINDOW_START_HOUR) {
    return h >= SERVICE_WINDOW_END_HOUR && h < SERVICE_WINDOW_START_HOUR;
  }
  return h < SERVICE_WINDOW_START_HOUR || h >= SERVICE_WINDOW_END_HOUR;
}

bool nextDepartureFarAway(const StreamSnapshot &snap, std::time_t now) {
  std::time_t soonest = 0;
  bool found = false;
  for (const StreamData &stream : snap.stream) {
    for (const Departure &d : stream.slot) {
      if (!d.valid) {
        continue;
      }
      if (!found || d.when < soonest) {
        soonest = d.when;
        found = true;
      }
    }
  }
  // No valid departure at all → treat as "far away" so Night can fire.
  if (!found) {
    return true;
  }
  return (soonest - now) > NIGHT_FIRST_DEP_MIN_AHEAD_S;
}

DisplayState selectDisplayState(const StreamSnapshot &snap,
                                const ScheduleSnapshot &schedule,
                                const PersistedMeta &meta,
                                const SelectorSignals &sig) {
  // Auth ahead of Boot: a cold boot with a broken AID should surface the
  // real problem, not the generic "loading…" screen.
  if (sig.auth_error_seen) {
    return DisplayState::Auth;
  }
  if (sig.first_render_ever && !meta.has_any_data) {
    return DisplayState::Boot;
  }
  if (!sig.wifi_up && (sig.now - sig.last_success) > OFFLINE_THRESHOLD_S) {
    return DisplayState::Offline;
  }
  if ((sig.now - sig.last_success) > STALE_THRESHOLD_V2_S) {
    return DisplayState::Stale;
  }
  // Quiet/Night must be decided against the *merged* view, not raw realtime.
  // Overnight the realtime window (~70 min) is empty, but the schedule hints
  // still carry tomorrow's first departures — falling through to Quiet here
  // would show "KEINE ABFAHRTEN" and throw those hints away. Merging first
  // means Quiet/Night only fire when even the plan has nothing to show.
  const StreamSnapshot merged = mergeSlots(snap, schedule, sig.now);
  if (allDeparturesBeyond(merged, sig.now + QUIET_HORIZON_S)) {
    return DisplayState::Quiet;
  }
  if (outsideServiceWindow(sig.now) && nextDepartureFarAway(merged, sig.now)) {
    return DisplayState::Night;
  }
  return DisplayState::Normal;
}

namespace {

void copyAidShort(const PersistedMeta & /*meta*/, char *dst, size_t cap) {
  // PersistedMeta does not (yet) persist the AID itself — only the streak
  // counter and the err flag. Schritt 7's Auth screen needs at least a
  // diagnostic substring; we surface a stable placeholder until the OGD/HAFAS
  // path begins persisting the live AID's first 8 chars (follow-up PR if
  // visual review in 11.8 deems it required).
  if (cap == 0) {
    return;
  }
  const char *placeholder = "AID:----";
  std::strncpy(dst, placeholder, cap - 1);
  dst[cap - 1] = '\0';
}

} // namespace

RenderInput composeRenderInput(DisplayState state, const StreamSnapshot &snap,
                               const ScheduleSnapshot &schedule,
                               const PersistedMeta &meta, std::time_t now) {
  RenderInput out;
  out.state = state;

  switch (state) {
  case DisplayState::Boot:
    out.firmware_version = DISPLAY_VERSION_STR;
    break;
  case DisplayState::Auth:
    copyAidShort(meta, out.auth_aid_short, sizeof(out.auth_aid_short));
    // No HTTP code persisted yet — leave 0; the Auth renderer prints
    // "ERR ---" when zero.
    out.auth_http_code = 0;
    break;
  case DisplayState::Offline: {
    out.last_fetch_at = meta.last_success_at;
    std::time_t elapsed =
        (now > meta.last_success_at) ? (now - meta.last_success_at) : 0;
    int rem = OFFLINE_THRESHOLD_S - static_cast<int>(elapsed);
    out.retry_in_s = (rem > 0) ? rem : 0;
    break;
  }
  case DisplayState::Stale:
    // Slots show "??:??" — driven by valid=false from the merger pass below
    // (Stale forces an empty snapshot, then no merge). Preserve the fetch's
    // api_ok so diagnostics don't misreport a reachable API as down.
    out.snapshot = StreamSnapshot{};
    out.snapshot.api_ok = snap.api_ok;
    break;
  case DisplayState::Night:
  case DisplayState::Normal:
    out.snapshot = mergeSlots(snap, schedule, now);
    break;
  case DisplayState::Quiet:
    // Fullscreen state — slot data is irrelevant, but keep the fetch's api_ok
    // so run.log reflects the real fetch result instead of the default false.
    out.snapshot.api_ok = snap.api_ok;
    break;
  }

  return out;
}

} // namespace bustaferl
