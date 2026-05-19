#include "efa_parse.h"

#include "string_util.h"
#include "time_constants.h"

#include <ArduinoJson.h>
#include <cstring>

#ifndef NATIVE_BUILD
#include <Stream.h>
#endif

namespace bustaferl {

namespace {

// JSON parser nesting limit; lifted from ArduinoJson's default (10) so the
// nested EFA response (departureList → servingLine → …) is not truncated.
constexpr int EFA_JSON_NESTING_LIMIT = 20;

int atoiSafe(const char *s) { return (s && *s) ? std::atoi(s) : 0; }

// EFA returns dateTime as { year, month, day, hour, minute } as decimal
// strings, in local time (Vienna), no timezone or seconds. Interpret as local
// time via mktime — the caller is responsible for having TZ set
// (Esp32Clock::setEnvTz on device, setenv("TZ", …) in tests).
// Returns 0 on parse failure.
time_t parseEfaDateTime(JsonObjectConst dt) {
  if (dt.isNull())
    return 0;
  // cppcheck-suppress badBitmaskCheck // ArduinoJson operator| (default value)
  const char *y = dt["year"] | (const char *)nullptr;
  // cppcheck-suppress badBitmaskCheck
  const char *mo = dt["month"] | (const char *)nullptr;
  // cppcheck-suppress badBitmaskCheck
  const char *d = dt["day"] | (const char *)nullptr;
  // cppcheck-suppress badBitmaskCheck
  const char *h = dt["hour"] | (const char *)nullptr;
  // cppcheck-suppress badBitmaskCheck
  const char *mi = dt["minute"] | (const char *)nullptr;
  if (!y || !mo || !d || !h || !mi)
    return 0;
  struct tm tm{};
  tm.tm_year = atoiSafe(y) - TM_YEAR_BASE;
  tm.tm_mon = atoiSafe(mo) - 1;
  tm.tm_mday = atoiSafe(d);
  tm.tm_hour = atoiSafe(h);
  tm.tm_min = atoiSafe(mi);
  tm.tm_sec = 0;
  tm.tm_isdst = -1; // let libc decide based on $TZ
  time_t t = mktime(&tm);
  return (t == (time_t)-1) ? 0 : t;
}

// Common parse-and-filter logic shared by the std::string and Stream
// overloads. `doc` must already hold the deserialized (filtered) EFA
// response. Mutates `hint` in place. The branch count tracks the
// stream/diva/line/direction match dimensions; splitting would inline-back
// or hide that this is one parse pass per departure.
// NOLINTNEXTLINE(readability-function-size)
void consumeEfaDoc(JsonDocument &doc, int call_diva,
                   const ScheduleStreamFilter (&filters)[STREAM_COUNT],
                   time_t cutoff, ScheduleHint (&hint)[STREAM_COUNT]) {
  for (int i = 0; i < STREAM_COUNT; ++i) {
    if (filters[i].diva == call_diva) {
      hint[i] = ScheduleHint{};
    }
  }

  int tomorrow_fill[STREAM_COUNT] = {0};

  JsonArrayConst deps = doc["departureList"].as<JsonArrayConst>();
  if (deps.isNull()) {
    return;
  }

  for (JsonObjectConst dep : deps) {
    JsonObjectConst sl = dep["servingLine"];
    // cppcheck-suppress badBitmaskCheck
    const char *number = sl["number"] | "";
    // cppcheck-suppress badBitmaskCheck
    const char *direction = sl["direction"] | "";
    time_t t = parseEfaDateTime(dep["dateTime"]);
    if (!t)
      continue;

    for (int i = 0; i < STREAM_COUNT; ++i) {
      if (filters[i].diva != call_diva)
        continue;
      if (filters[i].line != number)
        continue;
      const bool main_match =
          startsWith(direction, filters[i].direction_prefix);
      const bool alt_match =
          !filters[i].direction_prefix_alt.empty() &&
          startsWith(direction, filters[i].direction_prefix_alt);
      if (!main_match && !alt_match)
        continue;

      if (t < cutoff) {
        // Track the latest pre-cutoff match (`last_today`) plus the latest
        // two for `next_today` — the slot merger drops past times via its
        // own `t < now` filter, so what survives are the next today
        // departures still ahead. EFA returns chronological order, so each
        // subsequent hit shifts the rolling window of the last two.
        hint[i].last_today = t;
        hint[i].next_today[0] = hint[i].next_today[1];
        hint[i].next_today[1] = t;
      } else if (tomorrow_fill[i] < 2) {
        hint[i].first_tomorrow[tomorrow_fill[i]++] = t;
      }
      // first matching filter is enough — a single departure can't satisfy
      // two filters at the same stop anyway (distinct line+direction).
      break;
    }
  }
}

// EFA responses are ~37 KB and most of that is metadata we never read
// (servingLines block, route geometry, operator info, …). A filter drops
// everything except the fields the parser actually consumes, shrinking the
// resident JsonDocument by ~10× and keeping cold-boot heap usage in
// bounds.
void buildEfaFilter(JsonDocument &filter) {
  filter["departureList"][0]["dateTime"]["year"] = true;
  filter["departureList"][0]["dateTime"]["month"] = true;
  filter["departureList"][0]["dateTime"]["day"] = true;
  filter["departureList"][0]["dateTime"]["hour"] = true;
  filter["departureList"][0]["dateTime"]["minute"] = true;
  filter["departureList"][0]["servingLine"]["number"] = true;
  filter["departureList"][0]["servingLine"]["direction"] = true;
}

} // namespace

bool parseEfaResponse(const std::string &json, int call_diva,
                      const ScheduleStreamFilter (&filters)[STREAM_COUNT],
                      time_t cutoff, ScheduleHint (&hint)[STREAM_COUNT]) {
  JsonDocument filter;
  buildEfaFilter(filter);

  JsonDocument doc;
  auto err = deserializeJson(
      doc, json, DeserializationOption::Filter(filter),
      DeserializationOption::NestingLimit(EFA_JSON_NESTING_LIMIT));
  if (err)
    return false;

  consumeEfaDoc(doc, call_diva, filters, cutoff, hint);
  return true;
}

#ifndef NATIVE_BUILD
bool parseEfaResponse(::Stream &json, int call_diva,
                      const ScheduleStreamFilter (&filters)[STREAM_COUNT],
                      time_t cutoff, ScheduleHint (&hint)[STREAM_COUNT]) {
  JsonDocument filter;
  buildEfaFilter(filter);

  JsonDocument doc;
  auto err = deserializeJson(
      doc, json, DeserializationOption::Filter(filter),
      DeserializationOption::NestingLimit(EFA_JSON_NESTING_LIMIT));
  if (err)
    return false;

  consumeEfaDoc(doc, call_diva, filters, cutoff, hint);
  return true;
}
#endif

} // namespace bustaferl
