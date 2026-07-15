#ifndef BUSTAFERL_RENDER_NETWORK_PLAN_H
#define BUSTAFERL_RENDER_NETWORK_PLAN_H

#include "render/canvas.h"

namespace bustaferl {

// Network plan at the bottom of the departure board. Five-column schematic
// anchoring the user spatially (Hbf — Atzg — Ende — Tull — Hietz).
//
// Marker layout (column centres, returned in `centres_out[5]`):
//   0: Hbf       — 4×4 dot
//   1: Atzg      — 7×7 filled diamond (also bottom-row diamond)
//   2: Ende      — 4×4 dot
//   3: Tull      — 8×8 big square ("you are here")
//   4: Hietz     — 4×4 dot
constexpr int NETPLAN_COL_COUNT = 5;
constexpr int NETPLAN_HEIGHT = 52;

// Draws geometry + station labels. centres_out receives the column
// centres so tests / chained renderers can correlate further drawing.
void drawNetworkPlan(render::Canvas &canvas, int x, int y, int width,
                     int (&centres_out)[NETPLAN_COL_COUNT]);

// Convenience overload when centres aren't needed.
void drawNetworkPlan(render::Canvas &canvas, int x, int y, int width);

} // namespace bustaferl

#endif
