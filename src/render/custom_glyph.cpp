#include "render/custom_glyph.h"

namespace bustaferl {

namespace {

constexpr std::uint16_t BITS_PER_BYTE = 8;
constexpr std::uint16_t MSB_MASK = 0x80;

void drawGlyphRow(render::Canvas &canvas, int x, int y,
                  const std::uint8_t *row_bits, std::uint16_t w,
                  std::uint16_t ink) {
  for (std::uint16_t col = 0; col < w; ++col) {
    const std::uint8_t bit =
        row_bits[col >> 3] & (MSB_MASK >> (col & (BITS_PER_BYTE - 1)));
    if (bit) {
      canvas.drawPixel(x + col, y, ink);
    }
  }
}

} // namespace

void drawCustomGlyph(render::Canvas &canvas, int x, int y,
                     const GlyphBitmap &glyph, std::uint16_t ink) {
  const std::size_t stride = (glyph.w + (BITS_PER_BYTE - 1)) / BITS_PER_BYTE;
  for (std::uint16_t row = 0; row < glyph.h; ++row) {
    drawGlyphRow(canvas, x, y + row,
                 glyph.bits + static_cast<std::size_t>(row) * stride, glyph.w,
                 ink);
  }
}

} // namespace bustaferl
