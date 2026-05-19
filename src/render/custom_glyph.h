#ifndef BUSTAFERL_RENDER_CUSTOM_GLYPH_H
#define BUSTAFERL_RENDER_CUSTOM_GLYPH_H

#include "render/canvas.h"

#include <cstdint>

namespace bustaferl {

// 1-bit bitmap descriptor — keeps drawCustomGlyph's signature small.
struct GlyphBitmap {
  const std::uint8_t *bits;
  std::uint16_t w;
  std::uint16_t h;
};

// Draw a 1-bit bitmap (MSB-first per row, row stride = ceil(w/8)) onto a
// Canvas. Format matches what scripts/pbm-to-progmem.py emits. Used for
// the 90 px fullscreen-state glyphs (Boot / Offline / Auth screens) —
// U8g2 has no glyphs that large, so the migration plan keeps these as
// hand-shaped PROGMEM sprites (see §0.7 / C9).
void drawCustomGlyph(render::Canvas &canvas, int x, int y,
                     const GlyphBitmap &glyph, std::uint16_t ink = 1);

} // namespace bustaferl

#endif
