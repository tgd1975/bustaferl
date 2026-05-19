#include "render/plan_marker.h"

namespace bustaferl {

void drawPlanMark(render::Canvas &canvas, int x, int y) {
  constexpr int N = PLAN_MARK_SIZE;
  // 1 px stroke hollow square: paint the four edges in paper. fillRect
  // would draw the interior too, which we explicitly don't want — the
  // interior must stay ink so the marker reads as an empty box.
  canvas.drawFastHLine(x, y, N, 1);
  canvas.drawFastHLine(x, y + N - 1, N, 1);
  canvas.drawFastVLine(x, y, N, 1);
  canvas.drawFastVLine(x + N - 1, y, N, 1);
}

} // namespace bustaferl
