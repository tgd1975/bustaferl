#ifndef BUSTAFERL_SCHEDULE_REFRESH_H
#define BUSTAFERL_SCHEDULE_REFRESH_H

#include "data/ScheduleHint.h"
#include "logic/schedule_fetcher.h"

#include <ctime>

namespace bustaferl {

// Returns true if `s` should be re-fetched relative to `now`:
//   1. never fetched (fetched_at == 0)
//   2. older than SCHEDULE_HINT_MAX_AGE_S (48 h)
//   3. last_today has passed for some stream and we have not already
//      re-fetched after the current local midnight.
bool needScheduleRefresh(const ScheduleSnapshot &s, time_t now);

// Applies an EFA fetch result to an existing snapshot. Only overwrites a
// stream's hint slot if the result actually carries non-sentinel data for it
// (i.e. at least one of `first_tomorrow[0]` / `last_today` is set). This
// preserves previously-cached hints for streams whose DIVA call failed.
//
// Returns true iff the fetch was successful (r.ok == true). When true, the
// snapshot's `fetched_at` is stamped to `now`.
bool applyScheduleFetchResult(const ScheduleFetchResult &r, time_t now,
                              ScheduleSnapshot &out);

} // namespace bustaferl

#endif
