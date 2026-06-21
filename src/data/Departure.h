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
  // Optional per-slot line label, e.g. "S2", "S3", "REX1". Only the S-Bahn
  // stream fills it; bus streams leave it empty and the renderer falls back to
  // the static stream line. char[6] (not std::string) keeps Departure trivially
  // copyable and RTC-RAM friendly — it rides along in the RLE-persisted frame.
  // Holds up to 5 glyphs + NUL; longer labels are abbreviated to "xx" upstream.
  char line_label[6] = "";

  bool operator==(const Departure &o) const {
    return valid == o.valid && when == o.when && source == o.source &&
           std::strcmp(line_label, o.line_label) == 0;
  }
};

} // namespace bustaferl

#endif
