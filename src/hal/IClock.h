#ifndef BUSTAFERL_ICLOCK_H
#define BUSTAFERL_ICLOCK_H

#include <ctime>

namespace bustaferl {

// Lower bound for a "plausibly synced" wall-clock reading. Any epoch greater
// than or equal to this is taken as evidence that NTP (or another time
// source) has set the system clock; anything less is treated as
// seconds-since-boot from a deep-sleep wake.
//
// 1700000000 = 2023-11-14 22:13:20 UTC. Any value past 2023 is fine — the
// constant just has to be far enough in the future that a freshly-booted
// ESP32 (with no RTC backup battery) can never reach it without an NTP
// round-trip. Single source of truth; replaces the five hardcoded copies
// the codebase used to carry.
constexpr time_t MIN_PLAUSIBLE_EPOCH = 1700000000;

class IClock {
public:
  virtual ~IClock() = default;
  virtual time_t now() = 0;
  // Returns true on a successful sync.
  virtual bool ntpSync() = 0;
  // Last successful NTP sync, or 0 if never.
  virtual time_t lastSync() const = 0;
  // Default heuristic: synced iff the clock reads a plausible wall-clock
  // value. Host adapters whose clock is always wall-clock (libcurl runtime,
  // unit tests) override to return true unconditionally.
  virtual bool isSynced() { return now() >= MIN_PLAUSIBLE_EPOCH; }
};

} // namespace bustaferl

#endif
