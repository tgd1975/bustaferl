#ifndef BUSTAFERL_RENDER_INPUT_H
#define BUSTAFERL_RENDER_INPUT_H

#include "data/ScheduleHint.h"
#include "data/StreamSnapshot.h"
#include "render/layout.h"

#include <ctime>

namespace bustaferl {

// Assemble a RenderInput from the inputs the cycle has on hand. Encodes the
// stale-overrules-hints rule (a Stale overlay must keep showing "??:??", so
// the schedule must NOT leak through). For every other overlay we run the
// normal slot merger so cached hints can fill empty realtime slots.
//
// Caller passes `now` so the merger can drop past hints. No clock dependency
// inside — pure data transform, host-testable.
RenderInput composeRenderInput(const StreamSnapshot &snap,
                               const ScheduleSnapshot &schedule,
                               OverlayKind overlay, time_t now);

} // namespace bustaferl

#endif
