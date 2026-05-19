#include "render/bitmap_fonts.h"

#ifndef NATIVE_BUILD

#include <Adafruit_GFX.h>
#include <U8g2_for_Adafruit_GFX.h>

namespace bustaferl::render {

namespace {

// Single shared U8g2-bridge instance. The migration plan §0.6 path keeps
// U8g2_for_Adafruit_GFX as a stateless adapter — it stores a pointer to
// the current Adafruit_GFX canvas plus the active font and (per cycle)
// nothing else. One global is fine because the cycle is single-threaded
// and we never render two frames concurrently.
U8G2_FOR_ADAFRUIT_GFX g_u8g2;
Adafruit_GFX *g_attached = nullptr;

// Map FontRole → U8g2 font pointer. Logisoso covers the larger VT323-ish
// numeric sizes; helvR / 5x8 cover the Silkscreen-ish header sizes. The
// concrete pairings come from Risiko V8's plan (Schritt 7.3 may revisit
// after visual review in Schritt 11.8).
const uint8_t *fontFor(FontRole role) {
  switch (role) {
  case FontRole::TG_Row:
    return u8g2_font_logisoso28_tn; // 28 px, numeric-only — fast on ESP32
  case FontRole::EG_Row:
    return u8g2_font_logisoso22_tn; // 22 px numeric
  case FontRole::Atzg_Row:
    return u8g2_font_logisoso18_tn; // 18 px numeric
  case FontRole::Section_Header_TG:
    return u8g2_font_helvB12_tr;
  case FontRole::Section_Header_EG_Atzg:
    return u8g2_font_helvB10_tr;
  case FontRole::Network_Label:
    return u8g2_font_5x7_tr;
  case FontRole::Network_Arrow:
    return u8g2_font_open_iconic_arrow_1x_t;
  case FontRole::Badge_sm:
    return u8g2_font_helvB14_tr;
  case FontRole::Badge_md:
    return u8g2_font_helvB18_tr;
  case FontRole::Badge_lg:
    return u8g2_font_helvB24_tr;
  case FontRole::Fullscreen_Glyph_90:
    // 90 px sentinel: callers use drawCustomGlyph instead of U8g2 text. We
    // return a non-null small font so accidentally calling setRoleFont with
    // this role at least doesn't crash — but the resulting glyph won't be
    // the design's 90 px.
    return u8g2_font_helvB24_tr;
  case FontRole::Fullscreen_Title:
    return u8g2_font_helvB18_tr;
  case FontRole::Fullscreen_Sub:
    return u8g2_font_helvR14_tr;
  case FontRole::Fullscreen_Foot:
    return u8g2_font_helvR08_tr;
  }
  return u8g2_font_helvR08_tr;
}

} // namespace

void attachU8g2(Adafruit_GFX &canvas) {
  if (g_attached != &canvas) {
    g_u8g2.begin(canvas);
    g_attached = &canvas;
  }
}

void setRoleFont(Adafruit_GFX &canvas, FontRole role) {
  attachU8g2(canvas);
  g_u8g2.setFont(fontFor(role));
  // White-on-black is the global polarity (layout.cpp clears to black);
  // ink-pixel = white. setForegroundColor takes the canvas's colour space.
  g_u8g2.setForegroundColor(1);
}

} // namespace bustaferl::render

#endif // NATIVE_BUILD
