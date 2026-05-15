#include "stale_policy.h"

namespace bustaferl {

bool isStale(time_t last_success, time_t now, int threshold_s) {
  if (last_success <= 0)
    return true;
  if (now < last_success)
    return false; // clock anomaly, don't blank
  return (now - last_success) >= threshold_s;
}

} // namespace bustaferl
