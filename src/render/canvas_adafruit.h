#ifndef BUSTAFERL_RENDER_CANVAS_ADAFRUIT_H
#define BUSTAFERL_RENDER_CANVAS_ADAFRUIT_H

#ifndef NATIVE_BUILD

#include "render/canvas.h"

#include <Adafruit_GFX.h>
#include <U8g2_for_Adafruit_GFX.h>

namespace bustaferl::render {

// Concrete Canvas backed by Adafruit_GFX (geometry) + U8g2 (text). Wraps
// an externally-owned 1-bpp framebuffer; drawPixel writes 1 = paper / 0 =
// ink into the buffer with MSB = leftmost (matches FrameBuffer<>).
//
// One instance lives on the stack per renderFrame() call. The bridge to
// U8g2_for_Adafruit_GFX is held by reference so renderers can call
// setRoleFont() without re-binding every call.
class AdafruitGfxCanvas : public Canvas {
public:
  AdafruitGfxCanvas(std::uint8_t *buf, int w, int h);

  void drawPixel(int x, int y, std::uint16_t color) override;
  int width() const override { return w_; }
  int height() const override { return h_; }

  // Override the helpers — Adafruit_GFX's native fill/line are noticeably
  // faster than per-pixel loops.
  void fillRect(int x, int y, int w, int h, std::uint16_t color) override;
  void drawFastHLine(int x, int y, int w, std::uint16_t color) override;
  void drawFastVLine(int x, int y, int h, std::uint16_t color) override;
  void drawLine(int x0, int y0, int x1, int y1, std::uint16_t color) override;
  void drawRect(int x, int y, int w, int h, std::uint16_t color) override;

  void setCursor(int x, int y) override;
  void setTextColor(std::uint16_t color) override;
  void setRoleFont(FontRole role) override;
  void print(const char *text) override;

  // Access for code that still wants to talk to Adafruit_GFX directly
  // (drawCustomGlyph uses the .drawPixel() path so it does not need this).
  ::Adafruit_GFX &gfx() { return gfx_; }

private:
  // Inner Adafruit_GFX subclass that paints into our buffer.
  class Surface : public ::Adafruit_GFX {
  public:
    Surface(int16_t w, int16_t h, std::uint8_t *buf)
        : Adafruit_GFX(w, h), buf_(buf) {}
    void drawPixel(int16_t x, int16_t y, std::uint16_t color) override;

  private:
    std::uint8_t *buf_;
  };

  Surface gfx_;
  U8G2_FOR_ADAFRUIT_GFX u8g2_;
  int w_;
  int h_;
};

} // namespace bustaferl::render

#endif // NATIVE_BUILD

#endif
