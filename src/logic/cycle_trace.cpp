#include "logic/cycle_trace.h"

namespace bustaferl {

namespace {

// Advance a ring: write at head, bump head modulo cap, saturate count at cap.
template <typename Rec>
void ringPush(Rec *buf, std::uint8_t &head, std::uint8_t &count, int cap,
              const Rec &rec) {
  buf[head] = rec;
  head = static_cast<std::uint8_t>((head + 1) % cap);
  if (count < cap) {
    ++count;
  }
}

// Resolve the idx-th newest entry (idx 0 = most recent) into a ring index.
// Returns -1 when idx is out of range.
int newestIndex(std::uint8_t head, std::uint8_t count, int cap, int idx) {
  if (idx < 0 || idx >= count) {
    return -1;
  }
  // Most recent lives at head-1; walk backwards, wrapping.
  return ((head - 1 - idx) % cap + cap) % cap;
}

} // namespace

void tracePushCycle(CycleTrace &t, const CycleRecord &rec) {
  ringPush(t.cycles, t.cycle_head, t.cycle_count, CYCLE_TRACE_CAP, rec);
}

void tracePushError(CycleTrace &t, const ErrorRecord &rec) {
  ringPush(t.errors, t.error_head, t.error_count, ERROR_TRACE_CAP, rec);
}

const CycleRecord *traceCycleAt(const CycleTrace &t, int idx) {
  const int i = newestIndex(t.cycle_head, t.cycle_count, CYCLE_TRACE_CAP, idx);
  return i < 0 ? nullptr : &t.cycles[i];
}

const ErrorRecord *traceErrorAt(const CycleTrace &t, int idx) {
  const int i = newestIndex(t.error_head, t.error_count, ERROR_TRACE_CAP, idx);
  return i < 0 ? nullptr : &t.errors[i];
}

} // namespace bustaferl
