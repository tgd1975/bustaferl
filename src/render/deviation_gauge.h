#ifndef BUSTAFERL_RENDER_DEVIATION_GAUGE_H
#define BUSTAFERL_RENDER_DEVIATION_GAUGE_H

#include "render/canvas.h"

namespace bustaferl {

// Small vertical "live vs. Fahrplan" deviation gauge drawn next to a 58A
// departure time. It answers the trust question the bare HH:MM can't: is a
// time that jumped a live correction (bar off the Fahrplan baseline) or a
// data error? No numbers — legible purely as a shape, matching the board's
// 1-bit iconography. Scoped to the 58A rows; 58B and the S-Bahn are untouched.
//
// Geometry (px), all relative to the gauge top `top` and track centre `cx`:
//   * track      full height, 1 px
//   * zero line  the Fahrplan reference, the widest tick
//   * bar        grows up for "late" (positive), down for "early" (negative);
//                a 0-min agreement still shows a small nub, never bare ticks
//   * hollow square when there is no live match to compare against
//   * overflow tab just past the clamped end when the deviation is off-scale
constexpr int GAUGE_H = 24;       // total track height
constexpr int GAUGE_ZERO_DY = 15; // zero/Fahrplan baseline offset from the top
constexpr int GAUGE_SCALE = 3;    // px per minute of deviation
constexpr int GAUGE_UP_MIN = 5;   // headroom above zero (bus later / late)
constexpr int GAUGE_DOWN_MIN = 3; // headroom below zero (bus earlier)
// Horizontal footprint: the zero baseline is the widest element (7 px), so the
// gauge occupies cx-3 .. cx+3. Callers place `cx` so that stays in the margins.
constexpr int GAUGE_HALF_W = 3;

// Draw one gauge. `has_dev` false → the hollow "no live comparison" square;
// true → a bar for `dev_min` (minutes, positive = running late), clamped to
// the ±headroom with an overflow tab when it exceeds the drawn range.
void drawDeviationGauge(render::Canvas &canvas, int cx, int top, bool has_dev,
                        int dev_min);

} // namespace bustaferl

#endif
