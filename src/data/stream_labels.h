#ifndef BUSTAFERL_STREAM_LABELS_H
#define BUSTAFERL_STREAM_LABELS_H

#include "StreamSnapshot.h"

namespace bustaferl {

// Short human labels for the five streams, indexed by `Stream`. Used for
// per-slot log lines and the summary block. Single source of truth — v2 only
// has to touch this file (and STREAM_COUNT) to relabel/extend streams.
inline const char *streamLabel(int idx) {
  switch (idx) {
  case STREAM_58A_ATZ:
    return "58A-Atz";
  case STREAM_58A_HIETZING:
    return "58A-Hie";
  case STREAM_58B_ATZ:
    return "58B-Atz";
  case STREAM_U1_LEOPOLDAU:
    return "U1-Leo";
  case STREAM_U1_OBERLAA:
    return "U1-Obe";
  default:
    return "?";
  }
}

} // namespace bustaferl

#endif
