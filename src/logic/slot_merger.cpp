#include "slot_merger.h"

#include "../data/time_constants.h"

#include <algorithm>
#include <iterator>

namespace bustaferl {

namespace {

bool scheduleUsable(const ScheduleSnapshot &s, time_t now) {
  if (s.fetched_at == 0)
    return false;
  return (now - s.fetched_at) < SCHEDULE_HINT_MAX_AGE_S;
}

// Insert `cand` into the descending-merit (i.e. ascending-time) slot list
// `out`. Drops duplicates that round to the same wall-clock minute; realtime
// wins because realtime entries are pushed first (a Hint arriving for the
// same minute is silently dropped). A slot is considered occupied iff
// `valid`.
void insertSorted(Departure (&out)[SLOTS_PER_STREAM], const Departure &cand) {
  if (!cand.valid)
    return;
  const time_t cand_min = cand.when / SECONDS_PER_MINUTE;
  const bool duplicate =
      std::any_of(std::begin(out), std::end(out), [&](const Departure &slot) {
        return slot.valid && (slot.when / SECONDS_PER_MINUTE) == cand_min;
      });
  if (duplicate)
    return;
  for (int i = 0; i < SLOTS_PER_STREAM; ++i) {
    if (!out[i].valid) {
      out[i] = cand;
      // bubble back to maintain ascending order
      for (int j = i; j > 0 && out[j].when < out[j - 1].when; --j) {
        Departure tmp = out[j - 1];
        out[j - 1] = out[j];
        out[j] = tmp;
      }
      return;
    }
  }
  // All occupied — only keep if cand is earlier than the latest slot.
  int last = SLOTS_PER_STREAM - 1;
  if (cand.when < out[last].when) {
    out[last] = cand;
    for (int j = last; j > 0 && out[j].when < out[j - 1].when; --j) {
      Departure tmp = out[j - 1];
      out[j - 1] = out[j];
      out[j] = tmp;
    }
  }
}

} // namespace

StreamSnapshot mergeSlots(const StreamSnapshot &snap,
                          const ScheduleSnapshot &schedule, time_t now) {
  StreamSnapshot out =
      snap; // carries api_ok, endpoint_responded, filter_matched
  const bool use_schedule = scheduleUsable(schedule, now);

  for (int s = 0; s < STREAM_COUNT; ++s) {
    Departure merged[SLOTS_PER_STREAM]{};

    // Realtime slots first so they win on tie.
    for (const auto &d : snap.stream[s].slot) {
      if (d.valid && d.when >= now) {
        insertSorted(merged, d);
      }
    }

    if (use_schedule) {
      auto addHint = [&](time_t t) {
        if (t == 0 || t < now)
          return;
        Departure d;
        d.when = t;
        d.source = DepartureSource::Hint;
        d.valid = true;
        insertSorted(merged, d);
      };
      for (const time_t t : schedule.hint[s].next_today)
        addHint(t);
      for (const time_t t : schedule.hint[s].first_tomorrow)
        addHint(t);
    }

    for (int i = 0; i < SLOTS_PER_STREAM; ++i) {
      out.stream[s].slot[i] = merged[i];
    }
  }
  return out;
}

} // namespace bustaferl
