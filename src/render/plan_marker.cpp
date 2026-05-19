#include "render/plan_marker.h"

namespace bustaferl {

void drawPlanMark(Frame &fb, int x, int y) {
  constexpr int N = PLAN_MARK_SIZE;
  // 1 px stroke hollow square: paint the four edges in paper. fillRect
  // would draw the interior too, which we explicitly don't want — the
  // interior must stay ink so the marker reads as an empty box, not a
  // solid square.
  for (int i = 0; i < N; ++i) {
    fb.setPixel(x + i, y, true);         // top edge
    fb.setPixel(x + i, y + N - 1, true); // bottom edge
    fb.setPixel(x, y + i, true);         // left edge
    fb.setPixel(x + N - 1, y + i, true); // right edge
  }
}

} // namespace bustaferl
