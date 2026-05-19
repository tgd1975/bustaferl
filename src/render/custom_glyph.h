#ifndef BUSTAFERL_RENDER_CUSTOM_GLYPH_H
#define BUSTAFERL_RENDER_CUSTOM_GLYPH_H

#include <cstdint>

// Draw a 1-bit bitmap (MSB-first per row, row stride = ceil(w/8)) into an
// Adafruit_GFX-compatible canvas. The bitmap format matches what
// scripts/pbm-to-progmem.py emits. Used for the 90 px fullscreen-state glyphs
// (Boot / Offline / Auth screens) — U8g2 has no glyphs that large, so the
// migration plan keeps these as hand-shaped PROGMEM sprites (see §0.7 / C9).
//
// Host build: declared but not defined — render/custom_glyph.cpp depends on
// Adafruit_GFX which only compiles on ESP32 today.

#ifndef NATIVE_BUILD
class Adafruit_GFX;
#endif

namespace bustaferl {

#ifndef NATIVE_BUILD
void drawCustomGlyph(Adafruit_GFX &canvas, int x, int y, const uint8_t *bits,
                     uint16_t w, uint16_t h, uint16_t ink = 1);
#endif

} // namespace bustaferl

#endif
