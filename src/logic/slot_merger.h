#ifndef BUSTAFERL_SLOT_MERGER_H
#define BUSTAFERL_SLOT_MERGER_H

#include <ctime>

#include "../data/ScheduleHint.h"
#include "../data/StreamSnapshot.h"

namespace bustaferl {

// Maximum age of a ScheduleSnapshot before its hints are treated as expired
// and ignored. Concept §12.4.
constexpr time_t SCHEDULE_HINT_MAX_AGE_S = 48 * 3600;

// Returns a new snapshot whose per-stream slots are the chronologically first
// two future departures drawn from the union of `snap`'s realtime slots and
// the per-stream `schedule.hint[i].first_tomorrow[]` values. Realtime entries
// always win when they collide on time with a hint, so the user sees no
// transition when a morning departure crosses into the 70-min realtime window.
//
// `schedule` is ignored entirely if `schedule.fetched_at == 0` or older than
// SCHEDULE_HINT_MAX_AGE_S. Hint timestamps in the past are dropped.
//
// `now` is wall-clock time. All slots whose `when < now` are dropped.
StreamSnapshot mergeSlots(const StreamSnapshot &snap,
                          const ScheduleSnapshot &schedule, time_t now);

} // namespace bustaferl

#endif
