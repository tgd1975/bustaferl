#include "logic/render_input.h"

#include "config.h"
#include "logic/slot_merger.h"

#include <algorithm>
#include <cstring>

namespace bustaferl {

DisplayState selectDisplayState(const StreamSnapshot & /*snap*/,
                                const ScheduleSnapshot & /*schedule*/,
                                const PersistedMeta &meta,
                                const SelectorSignals &sig) {
  // Only four screens remain besides the departure board itself: the two
  // terminal error screens (Auth / WifiAuth — WifiAuth is chosen by the caller,
  // not here), the first-run Boot placeholder, and Offline. Everything else —
  // fresh data, only-scheduled data, stale/old data, quiet daytime gaps,
  // overnight — is the same board: it always shows the next departure (live or
  // scheduled) with its real time, or "--:--" only when even the schedule has
  // nothing. There is no separate Stale / Quiet / Night state; the data drives
  // the board, not a mode label.
  //
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
  case DisplayState::Normal:
    // The board: realtime merged with schedule hints. When realtime is empty
    // (fetch failed, or overnight with no live data) the merge falls back to
    // the scheduled departures, so the next departure still shows with its real
    // time; only slots the schedule can't fill render "--:--".
    out.snapshot = mergeSlots(snap, schedule, now);
    break;
  case DisplayState::WifiAuth:
    // Terminal wrong-password screen. Its only input (the configured SSID) is
    // set by the caller from the live network, not composed from the snapshot;
    // nothing to fill here.
    break;
  }

  return out;
}

} // namespace bustaferl
