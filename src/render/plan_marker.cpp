#include "render/plan_marker.h"

namespace bustaferl {

void drawPlanMark(render::Canvas &canvas, int x, int y) {
  // 5×5 hollow ring (rounded corners) — superscript "°" marker.
  //   .###.
  //   #...#
  //   #...#
  //   #...#
  //   .###.
  canvas.drawFastHLine(x + 1, y, 3, 1);
  canvas.drawFastHLine(x + 1, y + 4, 3, 1);
  canvas.drawFastVLine(x, y + 1, 3, 1);
  canvas.drawFastVLine(x + 4, y + 1, 3, 1);
}

} // namespace bustaferl
