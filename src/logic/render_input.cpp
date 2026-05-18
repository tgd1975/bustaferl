#include "logic/render_input.h"

#include "logic/slot_merger.h"

namespace bustaferl {

RenderInput composeRenderInput(const StreamSnapshot &snap,
                               const ScheduleSnapshot &schedule,
                               OverlayKind overlay, time_t now) {
  StreamSnapshot merged =
      (overlay == OverlayKind::Stale) ? snap : mergeSlots(snap, schedule, now);
  return RenderInput{merged, overlay};
}

} // namespace bustaferl
