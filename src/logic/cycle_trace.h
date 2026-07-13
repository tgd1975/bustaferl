#ifndef BUSTAFERL_LOGIC_CYCLE_TRACE_H
#define BUSTAFERL_LOGIC_CYCLE_TRACE_H

#include "../data/CycleTrace.h"

namespace bustaferl {

// Append a cycle record to the ring, overwriting the oldest once full.
void tracePushCycle(CycleTrace &t, const CycleRecord &rec);

// Append an anomaly record to the error ring.
void tracePushError(CycleTrace &t, const ErrorRecord &rec);

// Newest-first access: idx 0 is the most recent entry. Returns nullptr when
// idx is out of range (idx >= count). The diagnostic renderer walks these
// from 0 upward to list "neueste zuerst".
const CycleRecord *traceCycleAt(const CycleTrace &t, int idx);
const ErrorRecord *traceErrorAt(const CycleTrace &t, int idx);

} // namespace bustaferl

#endif
