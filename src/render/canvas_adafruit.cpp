#include "render/canvas_adafruit.h"

#ifndef NATIVE_BUILD

namespace bustaferl::render {

namespace {

// FontRole → U8g2 font pointer. Same table that used to live in
// bitmap_fonts.cpp; moved here because setRoleFont is now an instance
// method on the canvas.
const std::uint8_t *fontFor(FontRole role) {
  switch (role) {
  case FontRole::TG_Row:
    return u8g2_font_logisoso28_tn;
  case FontRole::EG_Row:
    return u8g2_font_logisoso22_tn;
  case FontRole::Atzg_Row:
    return u8g2_font_logisoso18_tn;
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

void AdafruitGfxCanvas::Surface::drawPixel(int16_t x, int16_t y,
                                           std::uint16_t color) {
  if (x < 0 || x >= _width || y < 0 || y >= _height) {
    return;
  }
  const std::size_t idx = static_cast<std::size_t>(y) * (_width / 8) + (x / 8);
  const std::uint8_t mask = 0x80 >> (x & 7);
  if (color) {
    buf_[idx] |= mask;
  } else {
    buf_[idx] &= ~mask;
  }
}

AdafruitGfxCanvas::AdafruitGfxCanvas(std::uint8_t *buf, int w, int h)
    : gfx_(static_cast<int16_t>(w), static_cast<int16_t>(h), buf), w_(w),
      h_(h) {
  u8g2_.begin(gfx_);
  u8g2_.setForegroundColor(1);
}

void AdafruitGfxCanvas::drawPixel(int x, int y, std::uint16_t color) {
  gfx_.drawPixel(static_cast<int16_t>(x), static_cast<int16_t>(y), color);
}

void AdafruitGfxCanvas::fillRect(int x, int y, int w, int h,
                                 std::uint16_t color) {
  gfx_.fillRect(static_cast<int16_t>(x), static_cast<int16_t>(y),
                static_cast<int16_t>(w), static_cast<int16_t>(h), color);
}

void AdafruitGfxCanvas::drawFastHLine(int x, int y, int w,
                                      std::uint16_t color) {
  gfx_.drawFastHLine(static_cast<int16_t>(x), static_cast<int16_t>(y),
                     static_cast<int16_t>(w), color);
}

void AdafruitGfxCanvas::drawFastVLine(int x, int y, int h,
                                      std::uint16_t color) {
  gfx_.drawFastVLine(static_cast<int16_t>(x), static_cast<int16_t>(y),
                     static_cast<int16_t>(h), color);
}

void AdafruitGfxCanvas::drawLine(int x0, int y0, int x1, int y1,
                                 std::uint16_t color) {
  gfx_.drawLine(static_cast<int16_t>(x0), static_cast<int16_t>(y0),
                static_cast<int16_t>(x1), static_cast<int16_t>(y1), color);
}

void AdafruitGfxCanvas::drawRect(int x, int y, int w, int h,
                                 std::uint16_t color) {
  gfx_.drawRect(static_cast<int16_t>(x), static_cast<int16_t>(y),
                static_cast<int16_t>(w), static_cast<int16_t>(h), color);
}

void AdafruitGfxCanvas::setCursor(int x, int y) {
  // U8g2_for_Adafruit_GFX's cursor convention is baseline-Y; our layout
  // constants use top-Y (per design handoff). Shift by the current font's
  // ascent so callers can think in top-Y everywhere. Assumes the font has
  // been selected via setRoleFont() before this call (true for our layout
  // pipeline: setRoleFont → setCursor → print).
  const int16_t ascent = u8g2_.getFontAscent();
  u8g2_.setCursor(static_cast<int16_t>(x), static_cast<int16_t>(y) + ascent);
}

void AdafruitGfxCanvas::setTextColor(std::uint16_t color) {
  u8g2_.setForegroundColor(color);
}

void AdafruitGfxCanvas::setRoleFont(FontRole role) {
  u8g2_.setFont(fontFor(role));
}

void AdafruitGfxCanvas::print(const char *text) { u8g2_.print(text); }

} // namespace bustaferl::render

#endif // NATIVE_BUILD
