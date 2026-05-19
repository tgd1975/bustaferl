#ifndef BUSTAFERL_RENDER_PLAN_MARKER_H
#define BUSTAFERL_RENDER_PLAN_MARKER_H

#include "frame_buffer.h"
#include "layout.h" // for Frame alias

namespace bustaferl {

// Plan marker: 5×5 px hollow square (1 px stroke) painted in `paper` over
// the global `ink` background. Renders next to a `--:--`-style time when
// the departure is plan-only (`Departure::source != Realtime`). Pure
// geometry — host-testable via FrameBuffer<>::getPixel.
//
// The 5×5 footprint matches the design handoff (Schritt 11.2 verifies it's
// big enough on real e-paper). Per Risiko V12 a 6×6 may follow if visually
// indistinguishable from a numeric glyph at the chosen font size.
void drawPlanMark(Frame &fb, int x, int y);

constexpr int PLAN_MARK_SIZE = 5;

} // namespace bustaferl

#endif
