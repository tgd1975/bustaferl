#include "render/custom_glyph.h"

#ifndef NATIVE_BUILD

#include <Adafruit_GFX.h>

namespace bustaferl {

void drawCustomGlyph(Adafruit_GFX &canvas, int x, int y, const uint8_t *bits,
                     uint16_t w, uint16_t h, uint16_t ink) {
  const uint16_t stride = (w + 7) / 8;
  for (uint16_t row = 0; row < h; ++row) {
    const uint8_t *src = bits + row * stride;
    for (uint16_t col = 0; col < w; ++col) {
      const uint8_t bit = src[col >> 3] & (0x80 >> (col & 7));
      if (bit) {
        canvas.drawPixel(x + col, y + row, ink);
      }
    }
  }
}

} // namespace bustaferl

#endif
