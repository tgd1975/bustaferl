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

// Pure function — picks the display state from the inputs. Only the error /
// placeholder screens are states now (Auth → Boot → Offline); everything with
// data — fresh, scheduled-only, stale, or a quiet gap — is the Normal board,
// where the merged data (not a mode label) decides what shows.
DisplayState selectDisplayState(const StreamSnapshot &snap,
                                const ScheduleSnapshot &schedule,
                                const PersistedMeta &meta,
                                const SelectorSignals &sig);

// Build a RenderInput for the chosen `state`. Fills the right slice of the
// struct (merged data for Normal, last_fetch/retry for Offline, AID/HTTP for
// Auth, version string for Boot). Pure function, no clock dep.
RenderInput composeRenderInput(DisplayState state, const StreamSnapshot &snap,
                               const ScheduleSnapshot &schedule,
                               const PersistedMeta &meta, std::time_t now);

} // namespace bustaferl

#endif
