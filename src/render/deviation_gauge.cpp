#include "render/deviation_gauge.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>

namespace bustaferl {

namespace {

// Paper = the visible foreground on this board (matches drawPlanMark / the
// times, which all draw in colour 1 over the ink background).
constexpr std::uint16_t MARK = 1;

constexpr int TICK_MINOR_W = 3; // ordinary minute tick
constexpr int TICK_MAJOR_W = 5; // every 5th minute
constexpr int ZERO_W = 7;       // Fahrplan baseline — the widest tick
constexpr int BAR_W = 5;        // deviation bar
constexpr int SQUARE = 6;       // hollow "no live comparison" marker
constexpr int TAB_W = 7;        // overflow tab
constexpr int TAB_H = 2;
constexpr int MIN_BAR_H = 2; // a 0-min agreement still shows a visible nub
constexpr int MAJOR_EVERY = 5;

void hTick(render::Canvas &canvas, int cx, int y, int w) {
  canvas.drawFastHLine(cx - w / 2, y, w, MARK);
}

} // namespace

void drawDeviationGauge(render::Canvas &canvas, int cx, int top, bool has_dev,
                        int dev_min) {
  const int zeroY = top + GAUGE_ZERO_DY;

  // 1. Track.
  canvas.drawFastVLine(cx, top, GAUGE_H, MARK);

  // 2. Minute ticks (skip zero; a wider tick every 5th minute).
  for (int mi = -GAUGE_DOWN_MIN; mi <= GAUGE_UP_MIN; ++mi) {
    if (mi == 0) {
      continue;
    }
    const int w = (mi % MAJOR_EVERY == 0) ? TICK_MAJOR_W : TICK_MINOR_W;
    hTick(canvas, cx, zeroY - mi * GAUGE_SCALE, w);
  }

  // 3. Zero / Fahrplan baseline (widest tick).
  hTick(canvas, cx, zeroY, ZERO_W);

  // 4. No live match to compare against → hollow square on the zero line,
  //    and nothing else. Every slot shows either this or a bar, never bare.
  if (!has_dev) {
    canvas.drawRect(cx - SQUARE / 2, zeroY - SQUARE / 2, SQUARE, SQUARE, MARK);
    return;
  }

  // 5. Live bar from the zero line toward the deviation (up = late).
  const int clamped =
      std::max(-GAUGE_DOWN_MIN, std::min(GAUGE_UP_MIN, dev_min));
  const int endY = zeroY - clamped * GAUGE_SCALE;
  int barY = std::min(endY, zeroY);
  int barH = std::abs(zeroY - endY);
  if (barH < MIN_BAR_H) {
    // Exactly on time (or sub-minute): a small nub straddling the baseline,
    // reinforcing "live tracking, currently agrees with Fahrplan".
    barH = MIN_BAR_H;
    barY = zeroY - MIN_BAR_H / 2;
  }
  canvas.fillRect(cx - BAR_W / 2, barY, BAR_W, barH, MARK);

  // Off-scale: a short tab just past the clamped track end so a clamped read
  // is visually distinct from a real in-range one.
  if (dev_min > GAUGE_UP_MIN) {
    canvas.fillRect(cx - TAB_W / 2, top - TAB_H, TAB_W, TAB_H, MARK);
  } else if (dev_min < -GAUGE_DOWN_MIN) {
    canvas.fillRect(cx - TAB_W / 2, top + GAUGE_H, TAB_W, TAB_H, MARK);
  }
}

} // namespace bustaferl
