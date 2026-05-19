#ifndef BUSTAFERL_RENDER_DISPLAY_STATE_H
#define BUSTAFERL_RENDER_DISPLAY_STATE_H

#include "frame_buffer.h"
#include "layout.h"

// Fullscreen renderers for the four "no data" DisplayStates: Boot, Offline,
// Auth, Quiet. Each clears the canvas to ink and paints a centred 90 px
// glyph (◌ / ! / §9 / —) with a title + sub + foot text below. Caller is
// renderFrame() in layout.cpp (Schritt 7.8).
//
// All four functions live on the ESP32-side only — they touch Adafruit_GFX
// for U8g2 text and drawCustomGlyph for the 90 px sprites. Host build sees
// only the declarations behind NATIVE_BUILD guards.

#include <ctime>

#ifndef NATIVE_BUILD

class Adafruit_GFX;

namespace bustaferl {

void drawBoot(Adafruit_GFX &canvas, const char *version_str);
void drawOffline(Adafruit_GFX &canvas, std::time_t last_fetch_at,
                 int retry_in_s);
void drawAuth(Adafruit_GFX &canvas, const char *aid_short, int http_code);
void drawQuiet(Adafruit_GFX &canvas);

} // namespace bustaferl

#endif // NATIVE_BUILD

#endif
