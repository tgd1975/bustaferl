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
  // The chronologically *last two* scheduled departures before the cutoff
  // (CONCEPT.md §12.3) — deliberately the tail of the evening, not "the next
  // two after now": the EFA query anchors at 22:00 and the hint is fetched
  // once a day, so no fixed pair could stay "next" all evening. The slot
  // merger drops entries with `t < now`, so as service winds down these
  // become exactly the departures still ahead — without them the display
  // showed "—:—" after the 70-min realtime window even though the EFA plan
  // still listed today's tail. Mid-evening they can name later departures
  // than the true next one, but only during a service gap wider than the
  // realtime window with two or more departures still to come.
  time_t next_today[2] = {0, 0};
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
