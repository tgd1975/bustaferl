#include "render/canvas.h"

namespace bustaferl::render {

void Canvas::fillRect(int x, int y, int w, int h, std::uint16_t color) {
  for (int yy = y; yy < y + h; ++yy) {
    for (int xx = x; xx < x + w; ++xx) {
      drawPixel(xx, yy, color);
    }
  }
}

void Canvas::drawFastHLine(int x, int y, int w, std::uint16_t color) {
  for (int xx = x; xx < x + w; ++xx) {
    drawPixel(xx, y, color);
  }
}

void Canvas::drawFastVLine(int x, int y, int h, std::uint16_t color) {
  for (int yy = y; yy < y + h; ++yy) {
    drawPixel(x, yy, color);
  }
}

void Canvas::drawRect(int x, int y, int w, int h, std::uint16_t color) {
  drawFastHLine(x, y, w, color);
  drawFastHLine(x, y + h - 1, w, color);
  drawFastVLine(x, y, h, color);
  drawFastVLine(x + w - 1, y, h, color);
}

void Canvas::drawLine(int x0, int y0, int x1, int y1, std::uint16_t color) {
  // Bresenham — good enough for the network plan's connecting lines.
  int dx = x1 > x0 ? x1 - x0 : x0 - x1;
  int dy = y1 > y0 ? y1 - y0 : y0 - y1;
  int sx = x0 < x1 ? 1 : -1;
  int sy = y0 < y1 ? 1 : -1;
  int err = dx - dy;
  for (;;) {
    drawPixel(x0, y0, color);
    if (x0 == x1 && y0 == y1) {
      break;
    }
    int e2 = err * 2;
    if (e2 > -dy) {
      err -= dy;
      x0 += sx;
    }
    if (e2 < dx) {
      err += dx;
      y0 += sy;
    }
  }
}

} // namespace bustaferl::render
