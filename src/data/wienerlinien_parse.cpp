#include "wienerlinien_parse.h"

#include "string_util.h"
#include "time_constants.h"

#include <ArduinoJson.h>
#include <cstring>

namespace bustaferl {

namespace {

// ISO8601 timestamp prefix length up to and including seconds, e.g.
// "2024-01-01T12:34:00" — 19 characters.
constexpr size_t ISO8601_DATETIME_PREFIX_LEN = 19;
constexpr int ISO8601_SSCANF_FIELD_COUNT = 6;

// Howard-Hinnant civil-from-days algorithm magic constants. See
// http://howardhinnant.github.io/date_algorithms.html
constexpr long CIVIL_DAYS_PER_ERA = 146097L; // 400 years in days
constexpr long CIVIL_DAYS_TO_EPOCH =
    719468L; // days from 0000-03-01 to 1970-01-01

// JSON parser nesting limit lifted from ArduinoJson's default (10) — Wiener
// Linien's locationStop block nests up to ~12 levels (Feature → geometry →
// coordinates …).
constexpr int WL_JSON_NESTING_LIMIT = 20;

} // namespace

// Days since civil epoch (1970-01-01) for y-m-d, after Howard Hinnant.
static long civil_to_days(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153u * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * CIVIL_DAYS_PER_ERA + static_cast<long>(doe) -
         CIVIL_DAYS_TO_EPOCH;
}

// Parses ISO8601 like "2024-01-01T12:34:00.000+0100" or "...Z".
// Returns 0 on failure.
static time_t parseIso8601(const char *s) {
  if (!s || std::strlen(s) < ISO8601_DATETIME_PREFIX_LEN)
    return 0;
  int year = 0;
  int mon = 0;
  int day = 0;
  int hour = 0;
  int min = 0;
  int sec = 0;
  if (sscanf(s, "%4d-%2d-%2dT%2d:%2d:%2d", &year, &mon, &day, &hour, &min,
             &sec) != ISO8601_SSCANF_FIELD_COUNT) {
    return 0;
  }
  long days = civil_to_days(year, mon, day);
  long t = days * SECONDS_PER_DAY + hour * SECONDS_PER_HOUR +
           min * SECONDS_PER_MINUTE + sec;

  // Find timezone marker (skip optional fractional seconds).
  const char *tz = s + ISO8601_DATETIME_PREFIX_LEN;
  const char *p = std::strchr(tz, 'Z');
  if (!p)
    p = std::strchr(tz, '+');
  if (!p)
    p = std::strchr(tz, '-');
  if (p && *p != 'Z') {
    int sign = (*p == '+') ? -1 : 1; // subtract offset to get UTC
    int oh = 0, om = 0;
    // accept "+HHMM" or "+HH:MM"
    if (sscanf(p + 1, "%2d:%2d", &oh, &om) != 2 &&
        sscanf(p + 1, "%2d%2d", &oh, &om) != 2) {
      return 0;
    }
    t += sign * (oh * SECONDS_PER_HOUR + om * SECONDS_PER_MINUTE);
  }
  return static_cast<time_t>(t);
}

static int findFilterForRbl(int rbl,
                            const StreamFilter (&filters)[STREAM_COUNT]) {
  for (int i = 0; i < STREAM_COUNT; ++i) {
    if (filters[i].rbl == rbl)
      return i;
  }
  return -1;
}

// The depth/branch count tracks the OGD response shape — monitors → lines →
// departures → time → fields. Refactor would either inline back via helpers
// taking 7+ args or fragment the parse into helpers that share private state.
// Splitting waits for the v2 HAFAS adapter (CONCEPT §v2-5.1) which restructures
// this parse top-to-bottom.
// NOLINTNEXTLINE(readability-function-size,readability-function-cognitive-complexity)
bool parseMonitorResponse(const std::string &json,
                          const StreamFilter (&filters)[STREAM_COUNT],
                          StreamSnapshot &out) {
  out = StreamSnapshot{};
  JsonDocument doc;
  // The OGD locationStop block nests Feature→geometry→coordinates etc. up to
  // ~12 levels; ArduinoJson's default cap is 10. Lift it for headroom.
  auto err = deserializeJson(
      doc, json, DeserializationOption::NestingLimit(WL_JSON_NESTING_LIMIT));
  if (err)
    return false;

  JsonArrayConst monitors = doc["data"]["monitors"].as<JsonArrayConst>();
  if (monitors.isNull()) {
    out.api_ok = true; // valid JSON, just nothing in it
    return true;
  }

  for (JsonObjectConst mon : monitors) {
    // cppcheck-suppress badBitmaskCheck  // ArduinoJson operator| (default
    // value)
    int rbl = mon["locationStop"]["properties"]["attributes"]["rbl"] | 0;
    int fi = findFilterForRbl(rbl, filters);
    if (fi < 0)
      continue;

    out.stream[fi].endpoint_responded = true;

    for (JsonObjectConst line : mon["lines"].as<JsonArrayConst>()) {
      const char *name = line["name"] | "";
      const char *towards = line["towards"] | "";
      if (filters[fi].line != name)
        continue;
      if (!startsWith(towards, filters[fi].towards_prefix))
        continue;

      int slot = 0;
      for (JsonObjectConst dep :
           line["departures"]["departure"].as<JsonArrayConst>()) {
        if (slot >= SLOTS_PER_STREAM)
          break;
        JsonObjectConst dt = dep["departureTime"];
        // cppcheck-suppress badBitmaskCheck  // ArduinoJson operator| (default
        // value)
        const char *real = dt["timeReal"] | (const char *)nullptr;
        // cppcheck-suppress badBitmaskCheck  // ArduinoJson operator| (default
        // value)
        const char *plan = dt["timePlanned"] | (const char *)nullptr;
        time_t t = 0;
        bool rt = false;
        if (real && *real) {
          t = parseIso8601(real);
          rt = (t != 0);
        }
        if (!t && plan && *plan) {
          t = parseIso8601(plan);
          rt = false;
        }
        if (!t)
          continue;

        out.stream[fi].slot[slot].when = t;
        out.stream[fi].slot[slot].source =
            rt ? DepartureSource::Realtime : DepartureSource::Plan;
        out.stream[fi].slot[slot].valid = true;
        ++slot;
      }
      if (slot > 0) {
        out.stream[fi].filter_matched = true;
      }
      break; // first matching line within a monitor is enough
    }
  }

  out.api_ok = true;
  return true;
}

} // namespace bustaferl
