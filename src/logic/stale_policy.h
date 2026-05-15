#ifndef BUSTAFERL_STALE_POLICY_H
#define BUSTAFERL_STALE_POLICY_H

#include <ctime>

namespace bustaferl {

// True iff the displayed data should be considered stale (i.e. clear all
// time slots, show "veraltet"). `threshold_s` is typically STALE_THRESHOLD_S.
bool isStale(time_t last_success, time_t now, int threshold_s);

}  // namespace bustaferl

#endif
