#include "wienerlinien_parse.h"

#include <ArduinoJson.h>
#include <cstring>

namespace bustaferl {

// Days since civil epoch (1970-01-01) for y-m-d, after Howard Hinnant.
static long civil_to_days(int y, unsigned m, unsigned d) {
    y -= m <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153u * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097L + static_cast<long>(doe) - 719468L;
}

// Parses ISO8601 like "2024-01-01T12:34:00.000+0100" or "...Z".
// Returns 0 on failure.
static time_t parseIso8601(const char* s) {
    if (!s || std::strlen(s) < 19) return 0;
    int  year, mon, day, hour, min, sec;
    if (sscanf(s, "%4d-%2d-%2dT%2d:%2d:%2d", &year, &mon, &day, &hour, &min,
               &sec) != 6) {
        return 0;
    }
    long days = civil_to_days(year, mon, day);
    long t    = days * 86400L + hour * 3600L + min * 60L + sec;

    // Find timezone marker (skip optional fractional seconds).
    const char* p = std::strchr(s + 19, 'Z');
    if (!p) p = std::strchr(s + 19, '+');
    if (!p) p = std::strchr(s + 19, '-');
    if (p && *p != 'Z') {
        int sign = (*p == '+') ? -1 : 1;  // subtract offset to get UTC
        int oh = 0, om = 0;
        // accept "+HHMM" or "+HH:MM"
        if (sscanf(p + 1, "%2d:%2d", &oh, &om) != 2 &&
            sscanf(p + 1, "%2d%2d", &oh, &om) != 2) {
            return 0;
        }
        t += sign * (oh * 3600L + om * 60L);
    }
    return static_cast<time_t>(t);
}

static int findFilterForRbl(int rbl,
                            const StreamFilter (&filters)[STREAM_COUNT]) {
    for (int i = 0; i < STREAM_COUNT; ++i) {
        if (filters[i].rbl == rbl) return i;
    }
    return -1;
}

static bool startsWith(const char* s, const std::string& prefix) {
    if (prefix.empty()) return true;
    if (!s) return false;
    return std::strncmp(s, prefix.c_str(), prefix.size()) == 0;
}

bool parseMonitorResponse(const std::string& json,
                          const StreamFilter (&filters)[STREAM_COUNT],
                          StreamSnapshot& out) {
    out = StreamSnapshot{};
    JsonDocument doc;
    auto err = deserializeJson(doc, json);
    if (err) return false;

    JsonArrayConst monitors = doc["data"]["monitors"].as<JsonArrayConst>();
    if (monitors.isNull()) {
        out.api_ok = true;  // valid JSON, just nothing in it
        return true;
    }

    for (JsonObjectConst mon : monitors) {
        int rbl = mon["locationStop"]["properties"]["attributes"]["rbl"] | 0;
        int fi  = findFilterForRbl(rbl, filters);
        if (fi < 0) continue;

        out.stream[fi].rbl_responded = true;

        for (JsonObjectConst line : mon["lines"].as<JsonArrayConst>()) {
            const char* name    = line["name"]      | "";
            const char* towards = line["towards"]   | "";
            if (filters[fi].line != name) continue;
            if (!startsWith(towards, filters[fi].towards_prefix)) continue;

            int slot = 0;
            for (JsonObjectConst dep :
                 line["departures"]["departure"].as<JsonArrayConst>()) {
                if (slot >= SLOTS_PER_STREAM) break;
                JsonObjectConst dt   = dep["departureTime"];
                const char*     real = dt["timeReal"]   | (const char*)nullptr;
                const char*     plan = dt["timePlanned"]| (const char*)nullptr;
                time_t t  = 0;
                bool   rt = false;
                if (real && *real) {
                    t  = parseIso8601(real);
                    rt = (t != 0);
                }
                if (!t && plan && *plan) {
                    t  = parseIso8601(plan);
                    rt = false;
                }
                if (!t) continue;

                out.stream[fi].slot[slot].when        = t;
                out.stream[fi].slot[slot].is_realtime = rt;
                out.stream[fi].slot[slot].valid       = true;
                ++slot;
            }
            if (slot > 0) {
                out.stream[fi].filter_matched = true;
            }
            break;  // first matching line within a monitor is enough
        }
    }

    out.api_ok = true;
    return true;
}

}  // namespace bustaferl
