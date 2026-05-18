#include "slot_merger.h"

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
// `out`. Drops duplicate `when` values; realtime wins ties because realtime
// entries are pushed first. A slot is considered occupied iff `valid`.
void insertSorted(Departure (&out)[SLOTS_PER_STREAM], const Departure &cand) {
  if (!cand.valid)
    return;
  const bool duplicate =
      std::any_of(std::begin(out), std::end(out), [&](const Departure &slot) {
        return slot.valid && slot.when == cand.when;
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
  StreamSnapshot out = snap; // carries api_ok, rbl_responded, filter_matched
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
      for (const time_t t : schedule.hint[s].first_tomorrow) {
        if (t == 0 || t < now)
          continue;
        Departure d;
        d.when = t;
        d.is_realtime = false;
        d.valid = true;
        insertSorted(merged, d);
      }
    }

    for (int i = 0; i < SLOTS_PER_STREAM; ++i) {
      out.stream[s].slot[i] = merged[i];
    }
  }
  return out;
}

} // namespace bustaferl
