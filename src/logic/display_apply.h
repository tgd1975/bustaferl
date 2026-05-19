#ifndef BUSTAFERL_DISPLAY_APPLY_H
#define BUSTAFERL_DISPLAY_APPLY_H

#include "../hal/IDisplay.h"
#include "../hal/IPersistentStore.h"
#include "refresh_planner.h"

#include <cstdint>
#include <ctime>

namespace bustaferl {

// Executes the RefreshDecision against the display and updates the persisted
// counters (partial_count, last_light_full, last_deep_clean). Pure glue —
// takes the IDisplay interface so it is testable with a FakeDisplay on host.
void applyDisplayDecision(IDisplay &display, const RefreshDecision &d,
                          const uint8_t *fb, PersistedMeta &meta, time_t now);

} // namespace bustaferl

#endif
