#ifndef BUSTAFERL_DEPARTURE_H
#define BUSTAFERL_DEPARTURE_H

#include "time_constants.h"

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
  time_t when = 0; // unix seconds, absolute (the displayed time)
  DepartureSource source = DepartureSource::Unknown;
  bool valid = false; // false → no departure for this slot
  // Short line name shown by the S-Bahn renderer (e.g. "S2", "REX1"). Bus
  // streams leave this empty — they have a fixed line per stream and don't
  // need a per-slot label. LINE_LABEL_CAP - 1 chars + null-terminator covers
  // all real values; anything longer is abbreviated to "xx" by the parser.
  static constexpr int LINE_LABEL_CAP = 6;
  char line_label[LINE_LABEL_CAP] = "";
  // Scheduled (timetable) time for this same departure, unix seconds. The OGD
  // realtime feed reports both `timePlanned` and `timeReal`; `when` carries the
  // real value when live, but the planned value is kept here so the 58A
  // deviation gauge can render live-minus-scheduled. 0 = no planned reference.
  // Deliberately the last member so existing positional aggregate inits
  // (`{when, source, valid}`) keep working. Not RTC-persisted (StreamSnapshot
  // lives in RAM, rebuilt each fetch), so it costs nothing against the budget.
  time_t planned = 0;

  // Convenience for the renderer: true if the realtime endpoint actually
  // produced a live time (vs. plan/hint fallback). Plan and Hint both render
  // as "non-live" with the plan-marker.
  bool live() const { return source == DepartureSource::Realtime; }

  // True when a live-vs-scheduled deviation can be shown: a live departure that
  // also carries a scheduled reference. The 58A deviation gauge draws a bar in
  // this case and a "no comparison" marker otherwise.
  bool hasDeviation() const { return live() && planned != 0; }

  // Live-minus-scheduled, rounded to whole minutes (positive = running late).
  // Only meaningful when hasDeviation(); returns 0 otherwise.
  int deviationMinutes() const {
    if (!hasDeviation()) {
      return 0;
    }
    const long diff = static_cast<long>(when) - static_cast<long>(planned);
    // Round to nearest minute (symmetric around zero).
    const long half =
        diff >= 0 ? SECONDS_PER_MINUTE / 2 : -(SECONDS_PER_MINUTE / 2);
    return static_cast<int>((diff + half) / SECONDS_PER_MINUTE);
  }

  bool operator==(const Departure &o) const {
    return valid == o.valid && when == o.when && source == o.source &&
           std::strcmp(line_label, o.line_label) == 0;
  }
};

} // namespace bustaferl

#endif
