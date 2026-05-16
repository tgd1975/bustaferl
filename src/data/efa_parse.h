#ifndef BUSTAFERL_EFA_PARSE_H
#define BUSTAFERL_EFA_PARSE_H

#include <string>

#include "ScheduleHint.h"
#include "StreamSnapshot.h"

namespace bustaferl {

// One filter per stream — like StreamFilter (OGD) but matches EFA fields:
// `servingLine.number` (line) and `servingLine.direction` (direction). Streams
// that share a Haltestelle share `diva`; one network call per unique DIVA
// suffices because the EFA response carries all lines/directions for a stop.
struct ScheduleStreamFilter {
  int diva = 0;
  std::string line;             // exact match against servingLine.number
  std::string direction_prefix; // prefix match against servingLine.direction
};

// Parses an EFA XSLT_DM_REQUEST JSON response and fills the streams whose
// `filters[i].diva == call_diva`. `cutoff` is the local-time boundary
// separating "today" from "tomorrow" — typically the next 03:00 after now().
//
// Departures with `dateTime` < cutoff feed `hint[i].last_today` (keeps the
// most recent match). Departures with `dateTime` >= cutoff feed
// `hint[i].first_tomorrow[0..1]` (first two matches in document order; EFA
// returns chronological order).
//
// Returns true if the JSON parsed cleanly (regardless of how many filters
// matched). Streams not addressed by this call are left untouched.
bool parseEfaResponse(const std::string &json, int call_diva,
                      const ScheduleStreamFilter (&filters)[STREAM_COUNT],
                      time_t cutoff, ScheduleHint (&hint)[STREAM_COUNT]);

} // namespace bustaferl

#endif
