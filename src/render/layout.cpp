#include "layout.h"

#ifndef NATIVE_BUILD

#include <Adafruit_GFX.h>
#include <cstdio>
#include <ctime>

namespace bustaferl {

namespace {

// Adafruit_GFX subclass that draws into an externally-owned 1-bpp framebuffer.
// Same layout as GFXcanvas1: row-major, MSB = leftmost, 1 = white, 0 = black.
class ExternalCanvas : public Adafruit_GFX {
public:
    ExternalCanvas(int16_t w, int16_t h, uint8_t* buf)
        : Adafruit_GFX(w, h), buf_(buf) {}

    void drawPixel(int16_t x, int16_t y, uint16_t color) override {
        if (x < 0 || x >= _width || y < 0 || y >= _height) return;
        const size_t  idx  = static_cast<size_t>(y) * (_width / 8) + (x / 8);
        const uint8_t mask = 0x80 >> (x & 7);
        if (color) buf_[idx] |= mask;
        else       buf_[idx] &= ~mask;
    }

private:
    uint8_t* buf_;
};

void formatHHMM(time_t t, int tz_offset, char* out, size_t cap) {
    if (cap < 6) return;
    time_t local = t + tz_offset;
    int    secs  = static_cast<int>(local % 86400);
    if (secs < 0) secs += 86400;
    int h = secs / 3600;
    int m = (secs % 3600) / 60;
    std::snprintf(out, cap, "%02d:%02d", h, m);
}

void drawText(ExternalCanvas& c, int x, int y, uint8_t size, const char* s) {
    c.setTextColor(0);  // black ink
    c.setTextSize(size);
    c.setCursor(x, y);
    c.print(s);
}

void drawHeader(ExternalCanvas& c, int y, const char* label) {
    drawText(c, 8, y, 2, label);
    c.drawFastHLine(8, y + 18, 384, 0);
}

void drawSlot(ExternalCanvas& c, int x, int y, const Departure& d,
              int tz_offset) {
    char buf[8];
    if (d.valid) {
        formatHHMM(d.when, tz_offset, buf, sizeof(buf));
    } else {
        std::snprintf(buf, sizeof(buf), "--:--");
    }
    drawText(c, x, y, 2, buf);
}

void drawStreamLine(ExternalCanvas& c, int y, const char* prefix,
                    const StreamData& s, int tz_offset) {
    drawText(c, 8, y, 2, prefix);
    drawSlot(c, 220, y, s.slot[0], tz_offset);
    drawSlot(c, 320, y, s.slot[1], tz_offset);
}

void drawOverlay(ExternalCanvas& c, OverlayKind kind) {
    const char* msg = nullptr;
    switch (kind) {
        case OverlayKind::Stale:       msg = "VERALTET";              break;
        case OverlayKind::FilterDead:  msg = "58B Filter ungueltig";  break;
        case OverlayKind::StartFailed: msg = "Start fehlgeschlagen";  break;
        default: return;
    }
    int y = 270;
    c.fillRect(0, y - 4, 400, 28, 1);   // clear white
    c.drawRect(4, y - 4, 392, 26, 0);
    drawText(c, 12, y, 2, msg);
}

}  // namespace

void renderFrame(const RenderInput& in, Frame& fb) {
    fb.clear(true);  // white background
    ExternalCanvas c(FB_W, FB_H, fb.data());

    drawHeader(c, 4, "TULLNERTALGASSE");
    drawStreamLine(c, 38, "58A -> Atzgers.",
                   in.snapshot.stream[STREAM_58A_ATZ], in.tz_offset_seconds);
    drawStreamLine(c, 66, "58A -> Hietzing",
                   in.snapshot.stream[STREAM_58A_HIETZING],
                   in.tz_offset_seconds);

    drawHeader(c, 110, "ENDEMANNGASSE");
    drawStreamLine(c, 144, "58B -> Atzgers.",
                   in.snapshot.stream[STREAM_58B_ATZ], in.tz_offset_seconds);
    drawText(c, 8, 174, 1, "(nach Schleife)");

    if (in.overlay != OverlayKind::None) drawOverlay(c, in.overlay);
}

}  // namespace bustaferl

#endif  // NATIVE_BUILD
