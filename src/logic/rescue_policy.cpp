#include "logic/rescue_policy.h"

namespace bustaferl {

RescueStep nextRescueStep(time_t now, time_t anchored_at,
                          const RescueConfig &cfg, unsigned &wait_s) {
  wait_s = 0;
  if (now < anchored_at)
    return RescueStep::Stop;
  const long elapsed = static_cast<long>(now - anchored_at);
  if (elapsed > cfg.window_end_s)
    return RescueStep::Stop;
  if (elapsed < cfg.window_start_s) {
    wait_s = static_cast<unsigned>(cfg.window_start_s - elapsed);
    return RescueStep::Wait;
  }
  return RescueStep::Retry;
}

} // namespace bustaferl
