#ifndef BUSTAFERL_SNAPSHOT_LOGGER_H
#define BUSTAFERL_SNAPSHOT_LOGGER_H

#include "data/Departure.h"
#include "data/StreamSnapshot.h"

#include <string>

namespace bustaferl {

// Three-letter provenance tag for a Departure. Mirrors `DepartureSource`.
const char *sourceTag(DepartureSource s);

// One log line for a single Departure slot. Mirrors the historic main.cpp
// `logSlot` format byte-for-byte:
//   "[api]   <tag>: --:--\n"                              (when invalid)
//   "[api]   <tag>: HH:MM <SRC> epoch=<seconds>\n"        (when valid)
std::string formatSlot(const char *tag, const Departure &d);

// 30-line summary block emitted after a snapshot fetch: one [api] header line
// with batch counters + per-stream endpoint/match flags, followed by 10
// per-slot log lines (5 streams x 2 slots) produced via formatSlot.
//
// Pulled out of fetchSnapshot so the formatting is host-testable and the
// stream labels live exactly once (data/stream_labels.h).
std::string formatSnapshotSummary(const StreamSnapshot &snap, int total_batches,
                                  int failed_batches);

} // namespace bustaferl

#endif
