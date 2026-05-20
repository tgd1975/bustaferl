#ifndef BUSTAFERL_RENDER_CANVAS_H
#define BUSTAFERL_RENDER_CANVAS_H

#include "bitmap_fonts.h" // FontRole

#include <cstdint>

namespace bustaferl::render {

// Abstract drawing surface. The ESP32 implementation wraps Adafruit_GFX +
// U8g2_for_Adafruit_GFX; the native implementation paints into a Frame
// directly with a small builtin font. Both targets see the same render/*
// code — no more `#ifndef NATIVE_BUILD` paths inside the render layer.
//
// Coordinate system: pixel (0,0) is top-left. The frame is 1 bpp; ink
// pixels are `false` in the underlying buffer, paper pixels are `true`,
// matching FrameBuffer's getPixel/setPixel polarity. This Canvas always
// takes a colour parameter as 1 = paper / 0 = ink, mirroring Adafruit_GFX
// so existing callsites stay readable.
class Canvas {
public:
  virtual ~Canvas() = default;

  // Pure virtual core — every concrete impl provides this.
  virtual void drawPixel(int x, int y, std::uint16_t color) = 0;
  virtual int width() const = 0;
  virtual int height() const = 0;

  // Default helper implementations layered on top of drawPixel(). Concrete
  // impls may override for speed (Adafruit_GFX has faster runs/rects).
  virtual void fillRect(int x, int y, int w, int h, std::uint16_t color);
  virtual void drawFastHLine(int x, int y, int w, std::uint16_t color);
  virtual void drawFastVLine(int x, int y, int h, std::uint16_t color);
  virtual void drawLine(int x0, int y0, int x1, int y1, std::uint16_t color);
  virtual void drawRect(int x, int y, int w, int h, std::uint16_t color);

  // Text API — minimum surface the v2 renderers need. Cursor + colour are
  // sticky between calls (Adafruit_GFX semantics).
  virtual void setCursor(int x, int y) = 0;
  virtual void setTextColor(std::uint16_t color) = 0;
  virtual void setRoleFont(FontRole role) = 0;
  virtual void print(const char *text) = 0;

  // Pixel width of `text` rendered with the currently selected role font.
  // Used by multi-column layouts (e.g. TG/EG's two-times row) that need to
  // know where the previous text ended before placing the next.
  virtual int textWidth(const char *text) = 0;
};

} // namespace bustaferl::render

#endif
