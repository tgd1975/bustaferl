#ifndef BUSTAFERL_RENDER_NETWORK_PLAN_H
#define BUSTAFERL_RENDER_NETWORK_PLAN_H

#include "frame_buffer.h"
#include "layout.h"

namespace bustaferl {

// Network plan at the bottom of the Normal/Stale/Night screens. Five-column
// schematic anchoring the user spatially (Hbf — Atzg — Ende — Tull — Hietz),
// drawn at (x, y) with the given width. Geometry only — the text labels
// underneath are drawn separately by drawNetworkPlanLabels() through U8g2.
//
// The split keeps the pixel-precise diamond / dot / line geometry host-
// testable; labels stay on the ESP32 because U8g2 is the only good way to
// render the 7 px Silkscreen-like font.
//
// Marker layout (column centres, returned in `centres_out[5]`):
//   0: Hbf       — 4×4 dot
//   1: Atzg      — 7×7 diamond (also bottom-row diamond)
//   2: Ende      — 4×4 dot
//   3: Tull      — 8×8 big square ("you are here")
//   4: Hietz     — 4×4 dot
constexpr int NETPLAN_COL_COUNT = 5;
constexpr int NETPLAN_HEIGHT = 36; // top row + vertical link + bottom row
constexpr int NETPLAN_LABEL_BAND_H = 10;

void drawNetworkPlanGeometry(Frame &fb, int x, int y, int width,
                             int (&centres_out)[NETPLAN_COL_COUNT]);

} // namespace bustaferl

#ifndef NATIVE_BUILD
class Adafruit_GFX;
namespace bustaferl {
// Full network plan: geometry + text labels via U8g2.
void drawNetworkPlan(Adafruit_GFX &canvas, Frame &fb, int x, int y, int width);
} // namespace bustaferl
#endif

#endif
