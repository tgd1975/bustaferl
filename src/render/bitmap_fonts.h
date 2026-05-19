#ifndef BUSTAFERL_RENDER_BITMAP_FONTS_H
#define BUSTAFERL_RENDER_BITMAP_FONTS_H

// Per the v2 migration plan §0.6 (Festlegung C8 = Option A): we drive text
// rendering through U8g2_for_Adafruit_GFX. This module owns the mapping from
// logical role → concrete U8g2 font, so every callsite in render/ asks for
// a role (`FontRole::TG_Row` etc.) and never names the U8g2 font directly.
// That keeps the day a Silkscreen / VT323 follow-up (Option B) lands a
// single-file change.
//
// Host-build note: the U8g2_for_Adafruit_GFX type only exists on ESP32.
// Native callers never need to render text — they assert on framebuffer
// bytes — so this header forwards just the role enum + the setRoleFont()
// signature behind NATIVE_BUILD.

namespace bustaferl {

// Logical font roles used across the v2 display. Sizes follow the design
// handoff (docs/design_handoff_display/README.md "Typography"). Comment
// captures the design-spec size; the concrete U8g2 font may be 1–2 px off
// due to library coverage (acknowledged drift per Risiko V8).
enum class FontRole {
  // Data rows (VT323-equivalent).
  TG_Row,   // 28 px — TULLNERTALGASSE row times + line label
  EG_Row,   // 22 px — ENDEMANNGASSE row
  Atzg_Row, // 20 px — ATZGERSDORF S-Bahn row

  // Section headers (Silkscreen-equivalent).
  Section_Header_TG,      // 12 px
  Section_Header_EG_Atzg, // 10 px

  // Network plan labels + arrow.
  Network_Label, //  7 px Silkscreen
  Network_Arrow, // 10 px VT323 (▼)

  // Line-badge text (badge.cpp). Three sizes from display.jsx.
  Badge_sm, // 14 px — S-Bahn row
  Badge_md, // 18 px — EG row
  Badge_lg, // 22 px — TG row

  // Fullscreen-state texts (display_state.cpp).
  Fullscreen_Glyph_90, // 90 px sentinel (not used for setRoleFont — Custom
                       // glyph path through render/custom_glyph.h)
  Fullscreen_Title,    // 18 px Silkscreen
  Fullscreen_Sub,      // 16 px VT323
  Fullscreen_Foot,     //  8 px Silkscreen
};

} // namespace bustaferl

#ifndef NATIVE_BUILD

class Adafruit_GFX;

namespace bustaferl::render {

// Initialise the U8g2 attachment lazily on first use. Idempotent. Must be
// called before any setRoleFont() that targets a real canvas. On ESP32 we
// build the U8g2_for_Adafruit_GFX once per cycle (it stores a back-pointer
// to the canvas) — the attach also re-arms the font cache.
void attachU8g2(Adafruit_GFX &canvas);

// Set the active font on `canvas` for the given role. After this call,
// canvas.print(...) draws at the role's size+typeface. Cursor stays where
// the caller placed it.
void setRoleFont(Adafruit_GFX &canvas, FontRole role);

} // namespace bustaferl::render

#endif // NATIVE_BUILD

#endif
