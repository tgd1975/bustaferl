#include "filter_health.h"

namespace bustaferl {

void FilterHealth::recordCall(bool rbl_responded, bool filter_matched) {
  if (!rbl_responded) {
    // No signal — leave streak untouched.
    return;
  }
  if (filter_matched) {
    streak_ = 0;
  } else if (streak_ < 0xFF) {
    ++streak_;
  }
}

} // namespace bustaferl
