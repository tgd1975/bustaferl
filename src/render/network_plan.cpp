#include "render/network_plan.h"

#include <cstdint>

namespace bustaferl {

namespace {

// Plan §7.6: 5×5 triangle ▼ as a custom sprite (C12). Plain MSB-first
// bitmap; the 5 columns fit in a single byte each.
constexpr int TRIANGLE_DOWN_SIZE = 5;
constexpr int TRIANGLE_OVERHEAD_Y = 9; // px above bottom row
// clang-format off
constexpr std::uint8_t TRIANGLE_DOWN_5[TRIANGLE_DOWN_SIZE] = {
    0b11111000,  // ###..
    0b01110000,  // .###.
    0b01110000,  // .###.
    0b00100000,  // ..#..
    0b00100000,  // ..#..
};
// clang-format on

void drawTriangleDown5(Frame &fb, int x, int y) {
  for (int row = 0; row < TRIANGLE_DOWN_SIZE; ++row) {
    const uint8_t bits = TRIANGLE_DOWN_5[row];
    for (int col = 0; col < TRIANGLE_DOWN_SIZE; ++col) {
      if (bits & (0x80 >> col)) {
        fb.setPixel(x + col, y + row, true);
      }
    }
  }
}

// Filled diamond (rotated square) of half-size r centred at (cx, cy). At
// 7×7 (r=3) the on-display ink mass reads visibly different from an
// outline-only stroke (Risiko V9). Each row spans [cx - (r - |dy|),
// cx + (r - |dy|)] inclusive.
void drawDiamond(Frame &fb, int cx, int cy, int half_size) {
  for (int dy = -half_size; dy <= half_size; ++dy) {
    int dx = half_size - (dy < 0 ? -dy : dy);
    for (int xo = -dx; xo <= dx; ++xo) {
      fb.setPixel(cx + xo, cy + dy, true);
    }
  }
}

void drawFilledRect(Frame &fb, int cx, int cy, int w, int h) {
  fb.fillRect(cx - w / 2, cy - h / 2, w, h, true);
}

void drawHLine(Frame &fb, int x0, int x1, int y) {
  if (x0 > x1) {
    int t = x0;
    x0 = x1;
    x1 = t;
  }
  for (int x = x0; x <= x1; ++x) {
    fb.setPixel(x, y, true);
  }
}

void drawVLine(Frame &fb, int x, int y0, int y1) {
  if (y0 > y1) {
    int t = y0;
    y0 = y1;
    y1 = t;
  }
  for (int y = y0; y <= y1; ++y) {
    fb.setPixel(x, y, true);
  }
}

} // namespace

void drawNetworkPlanGeometry(Frame &fb, int x, int y, int width,
                             int (&centres_out)[NETPLAN_COL_COUNT]) {
  // Column centres equally spaced across `width`. The five stations sit at
  // x + width * (i + 0.5) / 5.
  for (int i = 0; i < NETPLAN_COL_COUNT; ++i) {
    centres_out[i] = x + (width * (2 * i + 1)) / (2 * NETPLAN_COL_COUNT);
  }
  const int top_y = y + 5;
  const int bottom_y = y + NETPLAN_HEIGHT - 5;

  // Top row: dot at col 0 (Hbf), diamond at col 1 (Atzg). Connecting line.
  drawFilledRect(fb, centres_out[0], top_y, 4, 4);
  drawDiamond(fb, centres_out[1], top_y, 3); // 7×7 diamond
  drawHLine(fb, centres_out[0] + 2, centres_out[1] - 3, top_y);

  // Vertical link between the two Atzg diamonds.
  drawVLine(fb, centres_out[1], top_y + 3, bottom_y - 3);

  // Triangle ▼ over Tull column (5×5 sprite, C12).
  drawTriangleDown5(fb, centres_out[3] - TRIANGLE_DOWN_SIZE / 2,
                    bottom_y - TRIANGLE_OVERHEAD_Y);

  // Bottom row: diamond (Atzg) — dot (Ende) — big square (Tull) — dot
  // (Hietz). Lines connect them left-to-right.
  drawDiamond(fb, centres_out[1], bottom_y, 3);
  drawFilledRect(fb, centres_out[2], bottom_y, 4, 4);
  drawFilledRect(fb, centres_out[3], bottom_y, 8, 8);
  drawFilledRect(fb, centres_out[4], bottom_y, 4, 4);
  drawHLine(fb, centres_out[1] + 3, centres_out[2] - 2, bottom_y);
  drawHLine(fb, centres_out[2] + 2, centres_out[3] - 4, bottom_y);
  drawHLine(fb, centres_out[3] + 4, centres_out[4] - 2, bottom_y);
}

} // namespace bustaferl

#ifndef NATIVE_BUILD

#include "render/bitmap_fonts.h"

#include <Adafruit_GFX.h>
#include <cstring>

namespace bustaferl {

void drawNetworkPlan(Adafruit_GFX &canvas, Frame &fb, int x, int y, int width) {
  int centres[NETPLAN_COL_COUNT];
  drawNetworkPlanGeometry(fb, x, y, width, centres);

  // Labels row: 7 px Silkscreen-like, Tull + Atzg bold-emphasis (just by
  // virtue of the larger marker above them — the label font is uniform).
  render::setRoleFont(canvas, FontRole::Network_Label);
  canvas.setTextColor(1);
  const char *labels[NETPLAN_COL_COUNT] = {"Hbf", "Atzg", "Ende", "Tull",
                                           "Hietz"};
  const int label_y = y + NETPLAN_HEIGHT + 8;
  for (int i = 0; i < NETPLAN_COL_COUNT; ++i) {
    // Crude horizontal centring: assume ~3 px per char for the 5x7 font.
    int est_w = 3 * static_cast<int>(strlen(labels[i]));
    canvas.setCursor(centres[i] - est_w / 2, label_y);
    canvas.print(labels[i]);
  }
}

} // namespace bustaferl

#endif // NATIVE_BUILD
