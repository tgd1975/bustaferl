#ifndef BUSTAFERL_SCHEDULE_FETCHER_H
#define BUSTAFERL_SCHEDULE_FETCHER_H

#include "../data/ScheduleHint.h"
#include "../data/efa_parse.h"
#include "../hal/INetwork.h"

#include <ctime>
#include <string>

namespace bustaferl {

constexpr int DEFAULT_EFA_LIMIT = 50;
constexpr int DEFAULT_EFA_QUERY_HOUR = 22;
constexpr int DEFAULT_EFA_CUTOFF_HOUR = 3;

struct ScheduleFetchConfig {
  // EFA endpoint base — caller-supplied so tests can override it. Production
  // passes WL_EFA_DM_BASE from config.h.
  std::string endpoint_base;
  // limit passed to EFA: how many planned departures to request per call.
  // 50 comfortably spans evening + next morning at our stops.
  int limit = DEFAULT_EFA_LIMIT;
  // Local-time hour at which we anchor the EFA query window. 22:00 catches
  // both today's late departures and tomorrow's first ones in one response.
  int query_hour = DEFAULT_EFA_QUERY_HOUR;
  int query_minute = 0;
  // Cutoff between "today" and "tomorrow" for splitting hints, expressed as
  // local-time hour of the next day. 03:00 is past the latest Wiener-Linien
  // service and before the earliest morning bus.
  int cutoff_hour = DEFAULT_EFA_CUTOFF_HOUR;
};

struct ScheduleFetchResult {
  ScheduleHint hint[STREAM_COUNT];
  // Number of distinct DIVA calls attempted, and how many failed (HTTP or
  // parse). `ok` is true iff at least one call yielded a parsable response.
  int calls_attempted = 0;
  int calls_failed = 0;
  bool ok = false;
};

// Iterates the distinct DIVAs referenced by `filters`, performs one EFA GET
// per DIVA, and assembles the merged ScheduleHint array. `now` is the
// current wall-clock time (used to anchor the query date and the
// today/tomorrow cutoff). Per-call retry policy mirrors api_fetcher.
ScheduleFetchResult
fetchSchedule(INetwork &net, time_t now,
              const ScheduleStreamFilter (&filters)[STREAM_COUNT],
              const ScheduleFetchConfig &cfg);

// Build the EFA URL for one call. Exposed for tests; production code calls
// fetchSchedule.
std::string buildEfaUrl(const std::string &base, int diva, time_t query_time,
                        int limit);

// Compute the today/tomorrow cutoff (next local cutoff_hour) relative to
// `now`. Exposed for tests.
time_t computeCutoff(time_t now, int cutoff_hour);

} // namespace bustaferl

#endif
