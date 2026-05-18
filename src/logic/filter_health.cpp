#include "filter_health.h"

#include <limits>

namespace bustaferl {

void FilterHealth::recordCall(bool endpoint_responded, bool filter_matched) {
  if (!endpoint_responded) {
    // No signal — leave streak untouched.
    return;
  }
  if (filter_matched) {
    streak_ = 0;
  } else if (streak_ < std::numeric_limits<uint8_t>::max()) {
    ++streak_;
  }
}

} // namespace bustaferl
