#ifndef BUSTAFERL_OEBB_HAFAS_PARSE_H
#define BUSTAFERL_OEBB_HAFAS_PARSE_H

#include "StreamSnapshot.h"
#include "config.h"

#include <ctime>
#include <string>

#ifndef NATIVE_BUILD
#include <Stream.h>
#endif

namespace bustaferl {

// Single-stream filter for one HAFAS `mgate.exe` StationBoard request. The
// S-Bahn pipeline has exactly one filter (not a per-stream table like the OGD
// path) — see §2.2 of docs/v2-sbahn-migration-plan.md.
struct OebbStreamFilter {
  std::string stbloc_extid; // HAFAS extId, departure-board station
  std::string dirloc_extid; // HAFAS extId, downstream direction touch
  std::string products;     // jnyFltrL "value" bitmask, e.g. "63"
  int max_jny = OEBB_MAX_JNY;
};

// Builds the mgate.exe POST body for one StationBoard request. Pure function,
// host-testable. AID / client / ver are pulled from config.h.
std::string buildOebbRequest(const OebbStreamFilter &f);

// Per-call semantic outcome from parseOebbStationBoard. The return value of
// the parser tells you whether the JSON deserialized — these flags tell you
// what HAFAS actually said.
struct OebbParseResult {
  bool endpoint_responded = false; // err=="OK" AND svcResL[0].res non-null
  bool filter_matched = false;     // ≥1 surviving departure
  bool auth_error_seen = false;    // err ∈ {"AID","AUTH"}
};

// Parses an mgate.exe StationBoard response and writes up to
// SLOTS_PER_STREAM *future* departures (`when >= now`) into `out_stream` —
// HAFAS lists the just-departing train first, so skipping past ones keeps the
// stored slots genuinely upcoming. Source defaults to Realtime
// when `dTimeR`/`dDateR` are present, else Plan. `line_label` is filled from
// `prodL[…].nameS` with all whitespace stripped (e.g. "S 1" → "S1"). Strings
// longer than Departure::LINE_LABEL_CAP-1 chars are abbreviated to "xx".
//
// Returns true iff the JSON parsed cleanly (regardless of any API-level err);
// inspect `result` for semantic outcome:
//   - `endpoint_responded=true`  → HAFAS replied with err=="OK" and a non-null
//     svcResL[0].res block (jnyL may still be empty).
//   - `filter_matched=true`      → ≥1 non-cancelled departure landed in
//     `out_stream`.
//   - `auth_error_seen=true`     → err ∈ {"AID","AUTH"}; the State-Selector
//     uses this to drive the Auth screen.
bool parseOebbStationBoard(const std::string &json, time_t now,
                           StreamData &out_stream, OebbParseResult &result);

#ifndef NATIVE_BUILD
// Streaming overload: ArduinoJson pulls the body straight off the HTTP
// stream, so the ~30 KB response is never buffered in a std::string. That
// buffer was the last big contiguous allocation in a cycle and the one that
// aborted the chip when the server chunked the response (no Content-Length →
// geometric string growth → operator new failure → std::terminate).
//
// Fully-qualified `::Stream` because bustaferl has its own `Stream` enum
// (StreamSnapshot.h) — unqualified lookup inside the namespace finds that
// enum first and the Arduino class second.
bool parseOebbStationBoard(::Stream &json, time_t now, StreamData &out_stream,
                           OebbParseResult &result);
#endif

} // namespace bustaferl

#endif
