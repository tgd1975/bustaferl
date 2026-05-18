#ifndef BUSTAFERL_FILTER_BUILDER_H
#define BUSTAFERL_FILTER_BUILDER_H

#include "data/StreamSnapshot.h"
#include "data/efa_parse.h"
#include "data/wienerlinien_parse.h"

namespace bustaferl {

// Fills the OGD monitor stream filters (RBL + line + towards prefix) for the
// five streams the display shows. Protocol-neutral name — a v2 backend with
// different identifiers would replace the implementation, not the call site.
void buildStreamFilters(StreamFilter (&f)[STREAM_COUNT]);

// Fills the EFA schedule stream filters (DIVA + line + direction prefix) for
// the five streams the display shows.
void buildScheduleFilters(ScheduleStreamFilter (&f)[STREAM_COUNT]);

} // namespace bustaferl

#endif
