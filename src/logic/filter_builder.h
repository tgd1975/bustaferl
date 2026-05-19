#ifndef BUSTAFERL_FILTER_BUILDER_H
#define BUSTAFERL_FILTER_BUILDER_H

#include "data/StreamSnapshot.h"
#include "data/efa_parse.h"
#include "data/oebb_hafas_parse.h"
#include "data/wienerlinien_parse.h"

namespace bustaferl {

// Fills the OGD monitor stream filters (RBL + line + towards prefix) for the
// four streams the display shows. After v2 only the three bus streams are
// populated; STREAM_SBAHN_HBF stays default-constructed (rbl=0) — the parser
// keys off `rbl != 0` and skips it.
void buildStreamFilters(StreamFilter (&f)[STREAM_COUNT]);

// Fills the EFA schedule stream filters (DIVA + line + direction prefix).
// STREAM_SBAHN_HBF stays default-constructed (diva=0) — there is no EFA hint
// path for the S-Bahn (Variante 1 in §3.3 of the v2 migration plan), and
// schedule_fetcher's diva-skip-guard takes that as "no schedule call".
void buildScheduleFilters(ScheduleStreamFilter (&f)[STREAM_COUNT]);

// Builds the single OEBB HAFAS filter. There is exactly one S-Bahn stream,
// so this is a single-value filter (not a per-stream table). All values
// come from config.h.
OebbStreamFilter buildOebbFilter();

} // namespace bustaferl

#endif
