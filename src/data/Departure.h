#ifndef BUSTAFERL_DEPARTURE_H
#define BUSTAFERL_DEPARTURE_H

#include <ctime>

namespace bustaferl {

struct Departure {
  time_t when = 0;          // unix seconds, absolute
  bool is_realtime = false; // false → fell back to scheduled value
  bool valid = false;       // false → no departure for this slot

  bool operator==(const Departure &o) const {
    return valid == o.valid && when == o.when && is_realtime == o.is_realtime;
  }
};

} // namespace bustaferl

#endif
