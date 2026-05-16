#ifndef BUSTAFERL_STREAMSNAPSHOT_H
#define BUSTAFERL_STREAMSNAPSHOT_H

#include "Departure.h"

namespace bustaferl {

// Index into the three streams the display shows.
enum Stream {
  STREAM_58A_ATZ = 0,
  STREAM_58A_HIETZING = 1,
  STREAM_58B_ATZ = 2,
  STREAM_U1_LEOPOLDAU = 3,
  STREAM_U1_OBERLAA = 4,
  STREAM_COUNT = 5,
};

constexpr int SLOTS_PER_STREAM = 2;

struct StreamData {
  Departure slot[SLOTS_PER_STREAM];

  // True if the RBL was reachable and returned a well-formed payload, even
  // if the payload contained zero matching departures. Distinguishes
  // "RBL silent" from "filter mismatch" for filter-health tracking.
  bool rbl_responded = false;

  // True if at least one departure matched the line/towards filter.
  bool filter_matched = false;
};

struct StreamSnapshot {
  StreamData stream[STREAM_COUNT];

  // True iff the whole API call (HTTP + JSON parse) succeeded.
  bool api_ok = false;
};

} // namespace bustaferl

#endif
