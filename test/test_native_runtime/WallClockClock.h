#ifndef BUSTAFERL_NATIVE_RUNTIME_WALLCLOCK_H
#define BUSTAFERL_NATIVE_RUNTIME_WALLCLOCK_H

#include "../../src/hal/IClock.h"

#include <chrono>
#include <ctime>

namespace bustaferl::native_runtime {

// Host clock backed by std::chrono::system_clock. NTP-sync is a no-op because
// the host OS already keeps wall-clock time; isSynced() returns true
// unconditionally so the cycle's "is the clock plausible yet?" check
// short-circuits.
class WallClockClock : public IClock {
public:
  time_t now() override {
    return std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now());
  }
  bool ntpSync() override {
    last_sync_ = now();
    return true;
  }
  time_t lastSync() const override { return last_sync_; }
  bool isSynced() override { return true; }

private:
  time_t last_sync_ = 0;
};

} // namespace bustaferl::native_runtime

#endif
