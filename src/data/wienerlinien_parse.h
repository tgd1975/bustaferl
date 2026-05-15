#ifndef BUSTAFERL_WL_PARSE_H
#define BUSTAFERL_WL_PARSE_H

#include <string>

#include "StreamSnapshot.h"

namespace bustaferl {

// Filter for one stream within an OGD monitor response.
struct StreamFilter {
    int         rbl = 0;             // RBL number we expect this stream at
    std::string line;                // e.g. "58A"
    std::string towards_prefix;      // prefix match, case-sensitive; empty = no filter
};

// Parses an OGD monitor JSON response. The response contains a `monitors`
// array; each entry has a `locationStop` (with RBL) and a `lines` array
// whose departures we care about. For each StreamFilter[i] we fill
// snapshot.stream[i] with up to SLOTS_PER_STREAM departures.
//
// Returns true if the JSON parsed cleanly (regardless of how many filters
// matched). Sets snapshot.api_ok accordingly.
bool parseMonitorResponse(const std::string& json,
                          const StreamFilter (&filters)[STREAM_COUNT],
                          StreamSnapshot& out);

}  // namespace bustaferl

#endif
