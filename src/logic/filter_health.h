#ifndef BUSTAFERL_FILTER_HEALTH_H
#define BUSTAFERL_FILTER_HEALTH_H

#include <cstdint>

namespace bustaferl {

// Tracks consecutive successful API calls where a given RBL returned data
// but no departure matched the configured towards-filter. See CONCEPT.md §9.
class FilterHealth {
public:
    explicit FilterHealth(uint8_t dead_after = 3) : dead_after_(dead_after) {}

    // Called once per stream after each successful API call.
    void recordCall(bool rbl_responded, bool filter_matched);

    bool    isDead() const { return streak_ >= dead_after_; }
    uint8_t streak() const { return streak_; }
    void    reset() { streak_ = 0; }
    void    setStreak(uint8_t s) { streak_ = s; }

private:
    uint8_t streak_     = 0;
    uint8_t dead_after_ = 3;
};

}  // namespace bustaferl

#endif
