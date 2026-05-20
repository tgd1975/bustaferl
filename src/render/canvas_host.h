#ifndef BUSTAFERL_RENDER_CANVAS_HOST_H
#define BUSTAFERL_RENDER_CANVAS_HOST_H

#ifdef NATIVE_BUILD

#include "frame_buffer.h"
#include "layout.h"
#include "render/canvas.h"

#include <Adafruit_GFX.h>
#include <U8g2_for_Adafruit_GFX.h>

namespace bustaferl::render {

// Pixel-identical mirror of AdafruitGfxCanvas, running on the native host
// build via ArduinoFake. Same Adafruit_GFX geometry, same U8g2 fonts — the
// only difference is the backing framebuffer (host-side Frame vs. an
// externally-owned uint8_t*). PGM dumps from native tests now match what
// the device renders.
class HostCanvas : public Canvas {
public:
  explicit HostCanvas(Frame &fb);

  void drawPixel(int x, int y, std::uint16_t color) override;
  int width() const override { return Frame::width; }
  int height() const override { return Frame::height; }

  void fillRect(int x, int y, int w, int h, std::uint16_t color) override;
  void drawFastHLine(int x, int y, int w, std::uint16_t color) override;
  void drawFastVLine(int x, int y, int h, std::uint16_t color) override;
  void drawLine(int x0, int y0, int x1, int y1, std::uint16_t color) override;
  void drawRect(int x, int y, int w, int h, std::uint16_t color) override;

  void setCursor(int x, int y) override;
  void setTextColor(std::uint16_t color) override;
  void setRoleFont(FontRole role) override;
  void print(const char *text) override;
  int textWidth(const char *text) override;

private:
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
};

} // namespace bustaferl::render

#endif // NATIVE_BUILD

#endif
