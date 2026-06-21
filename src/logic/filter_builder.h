#ifndef BUSTAFERL_FILTER_BUILDER_H
#define BUSTAFERL_FILTER_BUILDER_H

#include "data/StreamSnapshot.h"
#include "data/efa_parse.h"
#include "data/oebb_hafas_parse.h"
#include "data/wienerlinien_parse.h"

namespace bustaferl {

// Fills the OGD monitor stream filters (RBL + line + towards prefix) for the
// three bus streams. The S-Bahn stream (STREAM_SBAHN_HBF) is left default
// (rbl = 0) — it is fetched via the ÖBB POST path, not the OGD monitor.
void buildStreamFilters(StreamFilter (&f)[STREAM_COUNT]);

// Fills the EFA schedule stream filters (DIVA + line + direction prefix) for
// the three bus streams. The S-Bahn stream is left default (diva = 0): no EFA
// hint path (CONCEPT §v2-8 Variante 1), so schedule_fetcher skips it.
void buildScheduleFilters(ScheduleStreamFilter (&f)[STREAM_COUNT]);

// Builds the single ÖBB HAFAS S-Bahn filter (EVA ids + product mask) from
// config.h. Fetched separately from the OGD batch via the POST path.
OebbStreamFilter buildOebbFilter();

} // namespace bustaferl

#endif
