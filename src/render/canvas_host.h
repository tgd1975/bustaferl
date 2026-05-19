#ifndef BUSTAFERL_RENDER_CANVAS_HOST_H
#define BUSTAFERL_RENDER_CANVAS_HOST_H

#include "frame_buffer.h"
#include "layout.h" // for Frame alias
#include "render/canvas.h"

namespace bustaferl::render {

// Host-only Canvas implementation. Paints directly into a 1-bpp
// framebuffer; text uses a builtin 5×7 ASCII bitmap font (close-enough
// per design-fidelity — not pixel-identical to U8g2). Used by native
// tests (test_native_render_all_states writes PGM dumps) and any future
// host runtime that wants to materialise a frame.
//
// Per FontRole the canvas picks a cell size (5×7 small, scaled-up for
// larger roles). Output is monospace ASCII; non-ASCII codepoints render
// as a generic placeholder block so the PGM dumps still show *where*
// the text lives, even if not exactly *what* it says.
class HostCanvas : public Canvas {
public:
  explicit HostCanvas(Frame &fb);

  void drawPixel(int x, int y, std::uint16_t color) override;
  int width() const override { return Frame::width; }
  int height() const override { return Frame::height; }

  void setCursor(int x, int y) override;
  void setTextColor(std::uint16_t color) override;
  void setRoleFont(FontRole role) override;
  void print(const char *text) override;

private:
  Frame &fb_;
  int cursor_x_ = 0;
  int cursor_y_ = 0;
  std::uint16_t text_color_ = 1;
  FontRole role_ = FontRole::Network_Label;
};

} // namespace bustaferl::render

#endif
