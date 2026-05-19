#ifndef BUSTAFERL_DEPARTURE_H
#define BUSTAFERL_DEPARTURE_H

#include <cstdint>
#include <cstring>
#include <ctime>

namespace bustaferl {

// Provenance of a single Departure. Protocol-agnostic on purpose — v2 backends
// (OEBB, additional EFA stops) will produce the same three flavours and reuse
// this enum unchanged.
enum class DepartureSource : std::uint8_t {
  Unknown = 0, // default — no provenance recorded
  Realtime,    // realtime endpoint reported a non-zero `timeReal`
  Plan,        // realtime endpoint fell back to the scheduled value
  Hint,        // injected by slot_merger from a ScheduleHint (EFA timetable)
};

struct Departure {
  time_t when = 0; // unix seconds, absolute
  DepartureSource source = DepartureSource::Unknown;
  bool valid = false; // false → no departure for this slot
  // Short line name shown by the S-Bahn renderer (e.g. "S2", "REX1"). Bus
  // streams leave this empty — they have a fixed line per stream and don't
  // need a per-slot label. LINE_LABEL_CAP - 1 chars + null-terminator covers
  // all real values; anything longer is abbreviated to "xx" by the parser.
  static constexpr int LINE_LABEL_CAP = 6;
  char line_label[LINE_LABEL_CAP] = "";

  // Convenience for the renderer: true if the realtime endpoint actually
  // produced a live time (vs. plan/hint fallback). Plan and Hint both render
  // as "non-live" with the plan-marker.
  bool live() const { return source == DepartureSource::Realtime; }

  bool operator==(const Departure &o) const {
    return valid == o.valid && when == o.when && source == o.source &&
           std::strcmp(line_label, o.line_label) == 0;
  }
};

} // namespace bustaferl

#endif
