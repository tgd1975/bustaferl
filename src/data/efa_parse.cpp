#include "efa_parse.h"

#include <ArduinoJson.h>
#include <cstring>

namespace bustaferl {

namespace {

bool startsWith(const char *s, const std::string &prefix) {
  if (prefix.empty())
    return true;
  if (!s)
    return false;
  return std::strncmp(s, prefix.c_str(), prefix.size()) == 0;
}

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
  tm.tm_year = atoiSafe(y) - 1900;
  tm.tm_mon = atoiSafe(mo) - 1;
  tm.tm_mday = atoiSafe(d);
  tm.tm_hour = atoiSafe(h);
  tm.tm_min = atoiSafe(mi);
  tm.tm_sec = 0;
  tm.tm_isdst = -1; // let libc decide based on $TZ
  time_t t = mktime(&tm);
  return (t == (time_t)-1) ? 0 : t;
}

} // namespace

bool parseEfaResponse(const std::string &json, int call_diva,
                      const ScheduleStreamFilter (&filters)[STREAM_COUNT],
                      time_t cutoff, ScheduleHint (&hint)[STREAM_COUNT]) {
  // EFA responses are ~37 KB and most of that is metadata we never read
  // (servingLines block, route geometry, operator info, …). A filter drops
  // everything except the fields the parser actually consumes, shrinking the
  // resident JsonDocument by ~10× and keeping cold-boot heap usage in
  // bounds.
  JsonDocument filter;
  filter["departureList"][0]["dateTime"]["year"] = true;
  filter["departureList"][0]["dateTime"]["month"] = true;
  filter["departureList"][0]["dateTime"]["day"] = true;
  filter["departureList"][0]["dateTime"]["hour"] = true;
  filter["departureList"][0]["dateTime"]["minute"] = true;
  filter["departureList"][0]["servingLine"]["number"] = true;
  filter["departureList"][0]["servingLine"]["direction"] = true;

  JsonDocument doc;
  auto err = deserializeJson(doc, json, DeserializationOption::Filter(filter),
                             DeserializationOption::NestingLimit(20));
  if (err)
    return false;

  // Reset hints for streams whose DIVA matches this call — we own them
  // exclusively for this response.
  for (int i = 0; i < STREAM_COUNT; ++i) {
    if (filters[i].diva == call_diva) {
      hint[i] = ScheduleHint{};
    }
  }

  // Track how many "tomorrow" slots we have filled per stream.
  int tomorrow_fill[STREAM_COUNT] = {0};

  JsonArrayConst deps = doc["departureList"].as<JsonArrayConst>();
  if (deps.isNull()) {
    // Valid JSON, no departures — leave hints zeroed and report success.
    return true;
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
      if (!startsWith(direction, filters[i].direction_prefix))
        continue;

      if (t < cutoff) {
        // Always keep the latest pre-cutoff match — EFA returns chronological
        // order, so each subsequent hit overwrites.
        hint[i].last_today = t;
      } else if (tomorrow_fill[i] < 2) {
        hint[i].first_tomorrow[tomorrow_fill[i]++] = t;
      }
      // first matching filter is enough — a single departure can't satisfy
      // two filters at the same stop anyway (distinct line+direction).
      break;
    }
  }

  return true;
}

} // namespace bustaferl
