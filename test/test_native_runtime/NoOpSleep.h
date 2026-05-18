#ifndef BUSTAFERL_NATIVE_RUNTIME_NOOPSLEEP_H
#define BUSTAFERL_NATIVE_RUNTIME_NOOPSLEEP_H

#include "../../src/hal/ISleep.h"

#include <chrono>
#include <cstdint>
#include <thread>

namespace bustaferl::native_runtime {

// Host sleep that actually sleeps. deepSleep() does NOT call std::exit — the
// native-runtime driver wants to keep running through many cycles inside one
// process so the host-side leak detector (valgrind / massif) sees the
// cumulative profile.
//
// Cold vs warm boot is signalled out-of-band: the very first wakeupCause()
// call after process start returns ColdBoot; subsequent calls return Timer.
// Button-wake is not exercised by the native runtime (see §9 "what step 9
// does NOT cover" — GPIO path is device-only).
//
// `time_scale` ≥ 1 keeps sleep durations honest (1.0 = wall-clock seconds);
// values < 1 compress them so the smoke target finishes in ~5 min instead
// of ~5 h at the production poll cadence.
class NoOpSleep : public ISleep {
public:
  explicit NoOpSleep(double time_scale = 1.0) : time_scale_(time_scale) {}

  WakeCause wakeupCause() override {
    if (first_call_) {
      first_call_ = false;
      return WakeCause::ColdBoot;
    }
    return WakeCause::Timer;
  }

  void deepSleep(unsigned seconds) override { sleepScaled(seconds); }
  void lightSleep(unsigned seconds) override { sleepScaled(seconds); }

private:
  void sleepScaled(unsigned seconds) {
    const auto ms = static_cast<std::int64_t>(
        static_cast<double>(seconds) * 1000.0 * time_scale_);
    if (ms <= 0)
      return;
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
  }

  bool first_call_ = true;
  double time_scale_;
};

} // namespace bustaferl::native_runtime

#endif
