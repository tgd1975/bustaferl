#include "Esp32Display.h"

#ifndef NATIVE_BUILD

#include "../config.h"

#include <GxEPD2_BW.h>
#include <cstring>

namespace bustaferl {

namespace {

// 400×300 UC8176 panel. GxEPD2_420 is the matching driver class.
using Panel = GxEPD2_BW<GxEPD2_420, GxEPD2_420::HEIGHT>;

// Push our 1-bpp framebuffer (1=white, 0=black) to the panel via
// Adafruit_GFX::drawBitmap. The 2-color overload is
// drawBitmap(x, y, bmp, w, h, color_for_set_bits, color_for_clear_bits)
// — so set bits (our white) must map to GxEPD_WHITE and clear bits
// (our black) to GxEPD_BLACK. Reversing these inverts every full/light
// refresh while leaving partial-refresh regions (which use drawPixel
// directly) correct — the cause of the previously observed "white
// rectangles on an inverted background" symptom.
void blit(Panel &panel, const uint8_t *fb) {
  panel.drawBitmap(0, 0, fb, EPD_WIDTH, EPD_HEIGHT, GxEPD_WHITE, GxEPD_BLACK);
}

void blitPartial(Panel &panel, const uint8_t *fb, const Bbox &b) {
  // Tile the bbox bytes out of the full framebuffer.
  const int stride = EPD_WIDTH / 8;
  for (int y = 0; y < b.h; ++y) {
    for (int xb = 0; xb < b.w / 8; ++xb) {
      uint8_t byte = fb[(b.y + y) * stride + (b.x / 8) + xb];
      for (int bit = 0; bit < 8; ++bit) {
        bool white = byte & (0x80 >> bit);
        panel.drawPixel(b.x + xb * 8 + bit, b.y + y,
                        white ? GxEPD_WHITE : GxEPD_BLACK);
      }
    }
  }
}

void bwFlash(Panel &panel) {
  panel.setFullWindow();
  panel.firstPage();
  do {
    panel.fillScreen(GxEPD_BLACK);
  } while (panel.nextPage());
  panel.firstPage();
  do {
    panel.fillScreen(GxEPD_WHITE);
  } while (panel.nextPage());
}

} // namespace

struct Esp32Display::Impl {
  Panel panel{GxEPD2_420(/*CS=*/EPD_CS, /*DC=*/EPD_DC, /*RST=*/EPD_RST,
                         /*BUSY=*/EPD_BUSY)};
};

Esp32Display::Esp32Display() : impl_(new Impl()) {}
Esp32Display::~Esp32Display() { delete impl_; }

void Esp32Display::init() { impl_->panel.init(115200, true, 2, false); }

void Esp32Display::drawFull(const uint8_t *fb) {
  impl_->panel.setFullWindow();
  impl_->panel.firstPage();
  do {
    impl_->panel.fillScreen(GxEPD_WHITE);
    blit(impl_->panel, fb);
  } while (impl_->panel.nextPage());
}

void Esp32Display::drawPartial(const uint8_t *fb, const Bbox &bbox) {
  if (bbox.empty())
    return;
  impl_->panel.setPartialWindow(bbox.x, bbox.y, bbox.w, bbox.h);

  // Local de-ghost: drive the changed region black then white before drawing
  // the new content. Partial refreshes reuse a differential waveform that
  // leaves faint ghosts of the previous pixels; cycling the window through
  // both extremes clears them without a full-screen flash — only the bbox
  // (typically the changed digits) blinks. If a single cycle proves too gentle
  // on this UC8176, bump to two.
  impl_->panel.firstPage();
  do {
    impl_->panel.fillScreen(GxEPD_BLACK);
  } while (impl_->panel.nextPage());
  impl_->panel.firstPage();
  do {
    impl_->panel.fillScreen(GxEPD_WHITE);
  } while (impl_->panel.nextPage());

  impl_->panel.firstPage();
  do {
    impl_->panel.fillScreen(GxEPD_WHITE);
    blitPartial(impl_->panel, fb, bbox);
  } while (impl_->panel.nextPage());
}

void Esp32Display::lightFull(const uint8_t *fb) {
  bwFlash(impl_->panel);
  drawFull(fb);
}

void Esp32Display::deepClean(const uint8_t *fb) {
  bwFlash(impl_->panel);
  bwFlash(impl_->panel);
  bwFlash(impl_->panel);
  drawFull(fb);
}

} // namespace bustaferl

#endif
