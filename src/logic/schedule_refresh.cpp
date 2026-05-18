#include "logic/schedule_refresh.h"

#include "logic/slot_merger.h"

#include <algorithm>
#include <iterator>

namespace bustaferl {

bool needScheduleRefresh(const ScheduleSnapshot &s, time_t now) {
  if (s.fetched_at == 0)
    return true;
  if (now - s.fetched_at >= SCHEDULE_HINT_MAX_AGE_S)
    return true;
  // Refresh once today's last departure has passed for any stream we still
  // have data for, and we have not already refreshed since today's local
  // midnight.
  struct tm local;
  localtime_r(&now, &local);
  local.tm_hour = 0;
  local.tm_min = 0;
  local.tm_sec = 0;
  local.tm_isdst = -1;
  time_t midnight = mktime(&local);
  if (s.fetched_at >= midnight)
    return false;
  return std::any_of(std::begin(s.hint), std::end(s.hint),
                     [now](const ScheduleHint &h) {
                       return h.last_today != 0 && now > h.last_today;
                     });
}

bool applyScheduleFetchResult(const ScheduleFetchResult &r, time_t now,
                              ScheduleSnapshot &out) {
  if (!r.ok)
    return false;
  // calls_failed > 0 means at least one DIVA missed; we cannot tell from
  // ScheduleFetchResult which one. Cheap-and-correct: only overwrite slot
  // if the parser actually wrote a non-zero first_tomorrow[0] or last_today
  // for it.
  for (int i = 0; i < STREAM_COUNT; ++i) {
    if (r.hint[i].first_tomorrow[0] != 0 || r.hint[i].last_today != 0) {
      out.hint[i] = r.hint[i];
    }
  }
  out.fetched_at = now;
  return true;
}

} // namespace bustaferl
