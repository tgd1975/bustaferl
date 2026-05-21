#ifndef BUSTAFERL_RENDER_PLAN_MARKER_H
#define BUSTAFERL_RENDER_PLAN_MARKER_H

#include "render/canvas.h"

namespace bustaferl {

// Plan marker: 5×5 px hollow ring (1 px stroke, rounded corners) painted in
// paper over the global ink background. Visually a small "°" / degree sign;
// renders superscript-style next to a `HH:MM` time when the departure is
// plan-only (`Departure::source != Realtime`).
void drawPlanMark(render::Canvas &canvas, int x, int y);

constexpr int PLAN_MARK_SIZE = 5;

} // namespace bustaferl

#endif
