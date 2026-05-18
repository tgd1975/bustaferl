#ifndef BUSTAFERL_SCHEDULEHINT_H
#define BUSTAFERL_SCHEDULEHINT_H

#include "StreamSnapshot.h"

#include <ctime>

namespace bustaferl {

// Per-stream scheduled hints loaded once a day from the EFA timetable API
// (see CONCEPT.md §12). Used to bridge the gap between today's last realtime
// departure and tomorrow's first one, so the display answers "wann fährt der
// Bus in der Früh?" in the evening — well before the 70-min realtime window.
struct ScheduleHint {
  // Last scheduled departure today. Renderer does not consume this directly;
  // it is the trigger used by the warm-cycle refresh logic to decide whether
  // the current snapshot is still valid for "today".
  time_t last_today = 0;
  // The two earliest scheduled departures of the next service day. Renderer
  // mixes these into the slot list whenever realtime data is short.
  time_t first_tomorrow[2] = {0, 0};
};

struct ScheduleSnapshot {
  ScheduleHint hint[STREAM_COUNT];
  // 0 ⇒ never loaded; otherwise the wall-clock time of the last successful
  // refresh. Consumers must treat data older than 48 h as stale.
  time_t fetched_at = 0;
};

} // namespace bustaferl

#endif
