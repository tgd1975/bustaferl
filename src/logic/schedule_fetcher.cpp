#include "schedule_fetcher.h"

#include <cstdio>

#include "api_fetcher.h"

#ifndef NATIVE_BUILD
#include <Arduino.h>
#define SCHED_LOG(...) Serial.printf(__VA_ARGS__)
#else
#define SCHED_LOG(...) ((void)0)
#endif

namespace bustaferl {

namespace {

// Returns true iff DIVA `d` appears in filters[0..i-1] — used to skip
// duplicate Haltestellen (Südtiroler Platz is shared by both U1 streams).
bool isDuplicateDiva(const ScheduleStreamFilter (&filters)[STREAM_COUNT],
                     int up_to, int d) {
  for (int j = 0; j < up_to; ++j) {
    if (filters[j].diva == d)
      return true;
  }
  return false;
}

} // namespace

std::string buildEfaUrl(const std::string &base, int diva, time_t query_time,
                        int limit) {
  struct tm local;
  localtime_r(&query_time, &local);
  char buf[160];
  std::snprintf(buf, sizeof(buf),
                "%s&name_dm=%d&itdDateDay=%02d&itdDateMonth=%02d"
                "&itdDateYear=%04d&itdTimeHour=%02d&itdTimeMinute=%02d"
                "&limit=%d",
                base.c_str(), diva, local.tm_mday, local.tm_mon + 1,
                local.tm_year + 1900, local.tm_hour, local.tm_min, limit);
  return std::string(buf);
}

time_t computeCutoff(time_t now, int cutoff_hour) {
  struct tm local;
  localtime_r(&now, &local);
  // Move to "tomorrow at cutoff_hour:00". If now is already past today's
  // cutoff_hour we still want the *next* one, so unconditionally +1 day.
  local.tm_mday += 1;
  local.tm_hour = cutoff_hour;
  local.tm_min = 0;
  local.tm_sec = 0;
  local.tm_isdst = -1;
  return mktime(&local);
}

ScheduleFetchResult
fetchSchedule(INetwork &net, time_t now,
              const ScheduleStreamFilter (&filters)[STREAM_COUNT],
              const ScheduleFetchConfig &cfg) {
  ScheduleFetchResult out;

  // Anchor query at today at query_hour:query_minute local. EFA returns
  // departures from that point forward, which spans the evening + next
  // morning we care about.
  time_t query_time;
  {
    struct tm local;
    localtime_r(&now, &local);
    local.tm_hour = cfg.query_hour;
    local.tm_min = cfg.query_minute;
    local.tm_sec = 0;
    local.tm_isdst = -1;
    query_time = mktime(&local);
  }
  time_t cutoff = computeCutoff(now, cfg.cutoff_hour);

  for (int i = 0; i < STREAM_COUNT; ++i) {
    int diva = filters[i].diva;
    if (diva == 0)
      continue;
    if (isDuplicateDiva(filters, i, diva))
      continue;

    std::string url = buildEfaUrl(cfg.endpoint_base, diva, query_time,
                                  cfg.limit);
    ++out.calls_attempted;

    std::string body;
    FetchConfig fc;
    FetchOutcome fo = fetchWithRetry(net, url, body, fc);
    if (!fo.ok) {
      SCHED_LOG("[sched] diva=%d httpGet failed after %d attempts\n", diva,
                    fo.attempts_taken);
      ++out.calls_failed;
      continue;
    }
    if (!parseEfaResponse(body, diva, filters, cutoff, out.hint)) {
      SCHED_LOG("[sched] diva=%d parse failed (%u bytes)\n", diva,
                    static_cast<unsigned>(body.size()));
      ++out.calls_failed;
      continue;
    }
  }

  out.ok = out.calls_attempted > 0 && out.calls_failed < out.calls_attempted;
  return out;
}

} // namespace bustaferl
