#ifndef BUSTAFERL_OEBB_HAFAS_PARSE_H
#define BUSTAFERL_OEBB_HAFAS_PARSE_H

#include "StreamSnapshot.h"

#include <string>

namespace bustaferl {

// Single-value filter for the one S-Bahn stream. Unlike the per-stream OGD
// StreamFilter table, there is exactly one ÖBB stream, so one struct (not a
// STREAM_COUNT array) carries its parameters — no unused rows to drag around.
struct OebbStreamFilter {
  std::string stb_eva;  // EVA id of the departure-board station (Atzgersdorf)
  std::string dir_eva;  // EVA id the journey must pass downstream (Wien Hbf)
  std::string products; // jnyFltrL PROD bitmask, e.g. "63"
  int max_jny = 6;
};

// Builds the mgate.exe StationBoard POST body for `f`. Pure function — AID,
// client and version come from config.h. Host-testable.
std::string buildOebbRequest(const OebbStreamFilter &f);

// Parses an mgate.exe StationBoard response into `out_stream` (reset first).
// Sets `endpoint_responded` iff `err == "OK"` and a non-null svcResL[0].res is
// present; `filter_matched` iff at least one non-cancelled departure survived.
// Each slot gets source=Realtime (when dTimeR present) or Plan (only dTimeS);
// the product name (e.g. "S2", "REX1") is copied into `line_label`, abbreviated
// to "xx" if longer than 5 chars. Times are local Vienna (mktime — caller sets
// $TZ). Returns true iff the JSON deserialized cleanly; an `err != "OK"` body
// still returns true (with endpoint_responded left false so auth-health trips).
bool parseOebbStationBoard(const std::string &json, StreamData &out_stream);

} // namespace bustaferl

#endif
