#ifndef BUSTAFERL_STREAM_LABELS_H
#define BUSTAFERL_STREAM_LABELS_H

#include "StreamSnapshot.h"

namespace bustaferl {

// Short human labels for the four streams, indexed by `Stream`. Used for
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
  case STREAM_SBAHN_HBF:
    return "SBahn-Hbf";
  default:
    return "?";
  }
}

// Renderer-side direction label. Static on purpose: the OGD `towards`
// string is too long and shifts occasionally ("Bhf. Atzgersdorf S (üb.
// Atzgersdorfer Str.)") — the display column needs a stable short form.
// "Atzgersdorf" fits unabbreviated: the label column ends well before the
// right-aligned time grid (verified against the host render). The S-Bahn
// stream returns "" because the S-Bahn header carries the direction.
inline const char *display_dir(int idx) {
  // Both Atzgersdorf-bound bus streams share the same display label;
  // Hietzing is its own. S-Bahn returns empty because the header carries
  // the direction. Switch-with-fallthrough triggered bugprone-branch-clone,
  // so resolve via early ifs — the small table makes that readable.
  if (idx == STREAM_58A_HIETZING) {
    return "Hietzing";
  }
  if (idx == STREAM_58A_ATZ || idx == STREAM_58B_ATZ) {
    return "Atzgersdorf";
  }
  return "";
}

} // namespace bustaferl

#endif
