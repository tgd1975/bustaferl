#ifndef BUSTAFERL_RENDER_INPUT_H
#define BUSTAFERL_RENDER_INPUT_H

#include "data/ScheduleHint.h"
#include "data/StreamSnapshot.h"
#include "hal/IPersistentStore.h"
#include "render/layout.h"

#include <ctime>

namespace bustaferl {

// Per-cycle observations fed into the v2 state-selector. Built fresh on each
// cycle by `cycle_runner` from the WiFi state, the clock, and the persisted
// success-counters. Decoupled from PersistedMeta so the helper can stay
// pure-function.
struct SelectorSignals {
  bool first_render_ever = false; // PersistedMeta::has_any_data == false
  bool auth_error_seen = false;   // HAFAS err=AID/AUTH OR 3x OGD 401/403
  bool wifi_up = false;
  std::time_t now = 0;          // Europe/Vienna local epoch
  std::time_t last_success = 0; // last successful end-to-end fetch (epoch)
};

// True if every valid departure across all streams lies strictly after
// `horizon`. Empty snapshots (no valid slot anywhere) also return true.
bool allDeparturesBeyond(const StreamSnapshot &snap, std::time_t horizon);

// True if `now` falls in the night gap defined by
// [SERVICE_WINDOW_END_HOUR, SERVICE_WINDOW_START_HOUR). With END<START the
// window wraps midnight (default 01:00–04:59 = night).
bool outsideServiceWindow(std::time_t now);

// True if the soonest valid plan departure across all streams is more than
// NIGHT_FIRST_DEP_MIN_AHEAD_S into the future. Guards Night-state against
// late-night services still running.
bool nextDepartureFarAway(const StreamSnapshot &snap, std::time_t now);

// Pure function — picks the v2 display state from the inputs. Decision order
// matches the design-handoff (Auth before Boot, Offline before Stale, etc.).
DisplayState selectDisplayState(const StreamSnapshot &snap,
                                const ScheduleSnapshot &schedule,
                                const PersistedMeta &meta,
                                const SelectorSignals &sig);

// Build a RenderInput for the chosen `state`. Fills the right slice of the
// struct (data fields for Normal/Stale/Night, last_fetch/retry for Offline,
// AID/HTTP for Auth, version string for Boot). Pure function, no clock dep.
RenderInput composeRenderInput(DisplayState state, const StreamSnapshot &snap,
                               const ScheduleSnapshot &schedule,
                               const PersistedMeta &meta, std::time_t now);

} // namespace bustaferl

#endif
