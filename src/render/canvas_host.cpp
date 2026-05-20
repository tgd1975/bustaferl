#ifdef NATIVE_BUILD

#include "render/canvas_host.h"

namespace bustaferl::render {

namespace {

// Same FontRole → U8g2 font pointer table as canvas_adafruit.cpp.
// Keep these two tables in sync so host PGM dumps match device renders.
const std::uint8_t *fontFor(FontRole role) {
  switch (role) {
  case FontRole::TG_Row:
    return u8g2_font_logisoso28_tn;
  case FontRole::EG_Row:
    return u8g2_font_logisoso22_tn;
  case FontRole::Atzg_Row:
    return u8g2_font_logisoso18_tn;
  case FontRole::Section_Header_TG:
    return u8g2_font_helvB12_te;
  case FontRole::Section_Header_EG_Atzg:
    return u8g2_font_helvB10_te;
  case FontRole::Network_Label:
    return u8g2_font_5x7_tr;
  case FontRole::Network_Arrow:
    return u8g2_font_open_iconic_arrow_1x_t;
  case FontRole::Badge_sm:
    return u8g2_font_helvB08_tr;
  case FontRole::Badge_md:
    return u8g2_font_helvB10_tr;
  case FontRole::Badge_lg:
    return u8g2_font_helvB14_tr;
  case FontRole::Fullscreen_Glyph_90:
    return u8g2_font_helvB24_tr;
  case FontRole::Fullscreen_Title:
    return u8g2_font_helvB18_te;
  case FontRole::Fullscreen_Sub:
    return u8g2_font_helvR14_te;
  case FontRole::Fullscreen_Foot:
    return u8g2_font_helvR08_te;
  }
  return u8g2_font_helvR08_tr;
}

} // namespace

void HostCanvas::Surface::drawPixel(int16_t x, int16_t y, std::uint16_t color) {
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

HostCanvas::HostCanvas(Frame &fb)
    : gfx_(static_cast<int16_t>(Frame::width),
           static_cast<int16_t>(Frame::height), fb.data()) {
  u8g2_.begin(gfx_);
  u8g2_.setForegroundColor(1);
}

void HostCanvas::drawPixel(int x, int y, std::uint16_t color) {
  gfx_.drawPixel(static_cast<int16_t>(x), static_cast<int16_t>(y), color);
}

void HostCanvas::fillRect(int x, int y, int w, int h, std::uint16_t color) {
  gfx_.fillRect(static_cast<int16_t>(x), static_cast<int16_t>(y),
                static_cast<int16_t>(w), static_cast<int16_t>(h), color);
}

void HostCanvas::drawFastHLine(int x, int y, int w, std::uint16_t color) {
  gfx_.drawFastHLine(static_cast<int16_t>(x), static_cast<int16_t>(y),
                     static_cast<int16_t>(w), color);
}

void HostCanvas::drawFastVLine(int x, int y, int h, std::uint16_t color) {
  gfx_.drawFastVLine(static_cast<int16_t>(x), static_cast<int16_t>(y),
                     static_cast<int16_t>(h), color);
}

void HostCanvas::drawLine(int x0, int y0, int x1, int y1, std::uint16_t color) {
  gfx_.drawLine(static_cast<int16_t>(x0), static_cast<int16_t>(y0),
                static_cast<int16_t>(x1), static_cast<int16_t>(y1), color);
}

void HostCanvas::drawRect(int x, int y, int w, int h, std::uint16_t color) {
  gfx_.drawRect(static_cast<int16_t>(x), static_cast<int16_t>(y),
                static_cast<int16_t>(w), static_cast<int16_t>(h), color);
}

void HostCanvas::setCursor(int x, int y) {
  // Mirror the cursor convention from canvas_adafruit.cpp:
  // U8g2_for_Adafruit_GFX treats y as the baseline; our layout constants use
  // top-Y. Shift by the current font's ascent so callers can think in top-Y
  // everywhere.
  // getFontAscent returns int8_t; cast through unsigned char first so
  // clang-tidy's bugprone-signed-char-misuse doesn't flag the int8_t → int
  // sign extension. Ascent is always non-negative in practice.
  const int ascent = static_cast<unsigned char>(u8g2_.getFontAscent());
  u8g2_.setCursor(static_cast<int16_t>(x), static_cast<int16_t>(y + ascent));
}

void HostCanvas::setTextColor(std::uint16_t color) {
  u8g2_.setForegroundColor(color);
}

void HostCanvas::setRoleFont(FontRole role) {
  u8g2_.setFont(fontFor(role));
  // u8g2_SetFont resets is_transparent to 0 on every font change — that paints
  // the whole glyph bbox with bg_color in addition to fg_color, which renders
  // any setForegroundColor(0) text as a solid ink rectangle. Force transparent
  // mode back on so glyphs are drawn over whatever's already in the canvas.
  u8g2_.setFontMode(1);
}

int HostCanvas::textWidth(const char *text) {
  if (text == nullptr) {
    return 0;
  }
  return static_cast<int>(u8g2_.getUTF8Width(text));
}

void HostCanvas::print(const char *text) {
  // Bypass Print::print(const char*) — ArduinoFake replaces it with a
  // mock-lookup that throws "Unknown instance" because our gfx_/u8g2_ aren't
  // registered FakeProxies. write(buf, size) is virtual and U8g2 overrides
  // it with a functional implementation, so we route through that.
  if (text == nullptr) {
    return;
  }
  std::size_t n = 0;
  while (text[n] != '\0') {
    ++n;
  }
  u8g2_.write(reinterpret_cast<const std::uint8_t *>(text), n);
}

} // namespace bustaferl::render

#endif
