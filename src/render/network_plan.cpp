#include "render/network_plan.h"

#include <cstdint>
#include <cstring>

namespace bustaferl {

namespace {

// 5×5 ▼ sprite (C12). Plain MSB-first bitmap.
constexpr int TRIANGLE_DOWN_SIZE = 5;
constexpr int TRIANGLE_OVERHEAD_Y = 9;
// clang-format off
constexpr std::uint8_t TRIANGLE_DOWN_5[TRIANGLE_DOWN_SIZE] = {
    0b11111000,
    0b01110000,
    0b01110000,
    0b00100000,
    0b00100000,
};
// clang-format on

void drawTriangleDown5(render::Canvas &canvas, int x, int y) {
  for (int row = 0; row < TRIANGLE_DOWN_SIZE; ++row) {
    const std::uint8_t bits = TRIANGLE_DOWN_5[row];
    for (int col = 0; col < TRIANGLE_DOWN_SIZE; ++col) {
      if (bits & (0x80 >> col)) {
        canvas.drawPixel(x + col, y + row, 1);
      }
    }
  }
}

// Filled diamond (rotated square) of half-size r centred at (cx, cy).
void drawDiamond(render::Canvas &canvas, int cx, int cy, int half_size) {
  for (int dy = -half_size; dy <= half_size; ++dy) {
    int dx = half_size - (dy < 0 ? -dy : dy);
    canvas.drawFastHLine(cx - dx, cy + dy, 2 * dx + 1, 1);
  }
}

void drawCenteredFilledRect(render::Canvas &canvas, int cx, int cy, int w,
                            int h) {
  canvas.fillRect(cx - w / 2, cy - h / 2, w, h, 1);
}

} // namespace

void drawNetworkPlan(render::Canvas &canvas, int x, int y, int width,
                     int (&centres_out)[NETPLAN_COL_COUNT]) {
  for (int i = 0; i < NETPLAN_COL_COUNT; ++i) {
    centres_out[i] = x + (width * (2 * i + 1)) / (2 * NETPLAN_COL_COUNT);
  }
  const int top_y = y + 5;
  const int bottom_y = y + NETPLAN_HEIGHT - 5;
  constexpr int DOT_SIZE = 5;
  constexpr int DIAMOND_HALF = 4;
  constexpr int TULL_SQUARE_SIZE = 8;

  // Top row: dot (Hbf) — line — diamond (Atzg).
  drawCenteredFilledRect(canvas, centres_out[0], top_y, DOT_SIZE, DOT_SIZE);
  drawDiamond(canvas, centres_out[1], top_y, DIAMOND_HALF);
  canvas.drawFastHLine(
      centres_out[0] + DOT_SIZE / 2 + 1, top_y,
      (centres_out[1] - DIAMOND_HALF) - (centres_out[0] + DOT_SIZE / 2 + 1), 1);

  // Vertical link between the two Atzg diamonds.
  canvas.drawFastVLine(centres_out[1], top_y + DIAMOND_HALF,
                       (bottom_y - DIAMOND_HALF) - (top_y + DIAMOND_HALF), 1);

  // Triangle ▼ over Tull column.
  drawTriangleDown5(canvas, centres_out[3] - TRIANGLE_DOWN_SIZE / 2,
                    bottom_y - TRIANGLE_OVERHEAD_Y);

  // Bottom row: diamond (Atzg) — dot (Ende) — big square (Tull) — dot (Hietz).
  drawDiamond(canvas, centres_out[1], bottom_y, DIAMOND_HALF);
  drawCenteredFilledRect(canvas, centres_out[2], bottom_y, DOT_SIZE, DOT_SIZE);
  drawCenteredFilledRect(canvas, centres_out[3], bottom_y, TULL_SQUARE_SIZE,
                         TULL_SQUARE_SIZE);
  drawCenteredFilledRect(canvas, centres_out[4], bottom_y, DOT_SIZE, DOT_SIZE);
  canvas.drawFastHLine(centres_out[1] + DIAMOND_HALF + 1, bottom_y,
                       (centres_out[2] - DOT_SIZE / 2 - 1) -
                           (centres_out[1] + DIAMOND_HALF + 1),
                       1);
  canvas.drawFastHLine(centres_out[2] + DOT_SIZE / 2 + 1, bottom_y,
                       (centres_out[3] - TULL_SQUARE_SIZE / 2 - 1) -
                           (centres_out[2] + DOT_SIZE / 2 + 1),
                       1);
  canvas.drawFastHLine(centres_out[3] + TULL_SQUARE_SIZE / 2 + 1, bottom_y,
                       (centres_out[4] - DOT_SIZE / 2 - 1) -
                           (centres_out[3] + TULL_SQUARE_SIZE / 2 + 1),
                       1);

  // Labels row.
  canvas.setRoleFont(FontRole::Network_Label);
  canvas.setTextColor(1);
  const char *labels[NETPLAN_COL_COUNT] = {"Hbf", "Atzg", "Ende", "Tull",
                                           "Hietz"};
  const int label_y = y + NETPLAN_HEIGHT + 8;
  for (int i = 0; i < NETPLAN_COL_COUNT; ++i) {
    int est_w = 3 * static_cast<int>(std::strlen(labels[i]));
    canvas.setCursor(centres_out[i] - est_w / 2, label_y);
    canvas.print(labels[i]);
  }
}

void drawNetworkPlan(render::Canvas &canvas, int x, int y, int width) {
  int centres[NETPLAN_COL_COUNT];
  drawNetworkPlan(canvas, x, y, width, centres);
}

} // namespace bustaferl
