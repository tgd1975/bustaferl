#include "render/display_state.h"

#ifndef NATIVE_BUILD

#include "render/bitmap_fonts.h"
#include "render/custom_glyph.h"
#include "render/glyph_dotted_circle_90.h"
#include "render/glyph_exclamation_90.h"
#include "render/glyph_para_nine_90.h"

#include <Adafruit_GFX.h>
#include <cstdio>
#include <cstring>
#include <ctime>

namespace bustaferl {

namespace {

constexpr int GLYPH_W = 90;
constexpr int GLYPH_H = 90;

// Centre the 90 px glyph horizontally on the 400 px canvas; vertical
// placement matches the design handoff (~64 px from the top).
constexpr int GLYPH_X = (FB_W - GLYPH_W) / 2;
constexpr int GLYPH_Y = 64;

// Text rows below the glyph, in design-handoff order.
constexpr int TITLE_Y = GLYPH_Y + GLYPH_H + 24;
constexpr int SUB_Y = TITLE_Y + 28;
constexpr int FOOT_Y = FB_H - 12;

void drawCentered(Adafruit_GFX &canvas, FontRole role, int y,
                  const char *text) {
  render::setRoleFont(canvas, role);
  canvas.setTextColor(1);
  // Crude horizontal centring — U8g2_for_Adafruit_GFX exposes
  // getUTF8Width(), use it for a tight result.
  // Without it we'd over-estimate; getUTF8Width is on the U8g2 bridge
  // attached via setRoleFont above.
  canvas.setCursor((FB_W - 0) / 2, y);
  // Fall back to a fixed-pixel-per-char estimate if the bridge can't
  // measure (network_plan does the same — ~6 px per ASCII char for
  // 14-18 px Helv).
  int est_w = 6 * static_cast<int>(std::strlen(text));
  canvas.setCursor((FB_W - est_w) / 2, y);
  canvas.print(text);
}

void formatHHMM(std::time_t t, char *out, size_t cap) {
  if (cap < 6) {
    return;
  }
  struct tm local{};
  localtime_r(&t, &local);
  std::snprintf(out, cap, "%02d:%02d", local.tm_hour, local.tm_min);
}

} // namespace

void drawBoot(Adafruit_GFX &canvas, const char *version_str) {
  drawCustomGlyph(canvas, GLYPH_X, GLYPH_Y, GLYPH_DOTTED_CIRCLE_90, GLYPH_W,
                  GLYPH_H);
  drawCentered(canvas, FontRole::Fullscreen_Title, TITLE_Y, "bustaferl");
  drawCentered(canvas, FontRole::Fullscreen_Sub, SUB_Y, "laedt Fahrplan...");
  if (version_str != nullptr) {
    drawCentered(canvas, FontRole::Fullscreen_Foot, FOOT_Y, version_str);
  }
}

void drawOffline(Adafruit_GFX &canvas, std::time_t last_fetch_at,
                 int retry_in_s) {
  drawCustomGlyph(canvas, GLYPH_X, GLYPH_Y, GLYPH_EXCLAMATION_90, GLYPH_W,
                  GLYPH_H);
  drawCentered(canvas, FontRole::Fullscreen_Title, TITLE_Y, "Kein Empfang");

  char sub_buf[40] = "Noch nie aktualisiert";
  if (last_fetch_at > 0) {
    char hhmm[8];
    formatHHMM(last_fetch_at, hhmm, sizeof(hhmm));
    std::snprintf(sub_buf, sizeof(sub_buf), "Letzte Aktualisierung %s", hhmm);
  }
  drawCentered(canvas, FontRole::Fullscreen_Sub, SUB_Y, sub_buf);

  char foot_buf[40];
  if (retry_in_s > 0) {
    std::snprintf(foot_buf, sizeof(foot_buf), "WLAN - Retry in %ds",
                  retry_in_s);
  } else {
    std::snprintf(foot_buf, sizeof(foot_buf), "WLAN - retrying...");
  }
  drawCentered(canvas, FontRole::Fullscreen_Foot, FOOT_Y, foot_buf);
}

void drawAuth(Adafruit_GFX &canvas, const char *aid_short, int http_code) {
  drawCustomGlyph(canvas, GLYPH_X, GLYPH_Y, GLYPH_PARA_NINE_90, GLYPH_W,
                  GLYPH_H);
  drawCentered(canvas, FontRole::Fullscreen_Title, TITLE_Y, "Auth-Fehler");
  drawCentered(canvas, FontRole::Fullscreen_Sub, SUB_Y,
               "Client-ID veraltet - bitte neu registrieren");

  char foot_buf[48];
  const char *aid =
      (aid_short != nullptr && aid_short[0] != '\0') ? aid_short : "AID:---";
  if (http_code > 0) {
    std::snprintf(foot_buf, sizeof(foot_buf), "%s - ERR %d", aid, http_code);
  } else {
    std::snprintf(foot_buf, sizeof(foot_buf), "%s - ERR ---", aid);
  }
  drawCentered(canvas, FontRole::Fullscreen_Foot, FOOT_Y, foot_buf);
}

void drawQuiet(Adafruit_GFX &canvas) {
  // Quiet uses a plain horizontal-rule glyph "—" instead of a custom
  // bitmap. Big text-rendered "—" via Fullscreen_Sub is good enough; the
  // design handoff calls for 72 px which U8g2 doesn't have at exactly
  // that size — Logisoso24 reads as a heavy dash at the right scale.
  render::setRoleFont(canvas, FontRole::Fullscreen_Title);
  canvas.setTextColor(1);
  canvas.setCursor(FB_W / 2 - 30, GLYPH_Y + GLYPH_H / 2 + 12);
  canvas.print("---");
  drawCentered(canvas, FontRole::Fullscreen_Title, TITLE_Y, "Keine Abfahrten");
  drawCentered(canvas, FontRole::Fullscreen_Sub, SUB_Y,
               "in den naechsten 20 min");
}

} // namespace bustaferl

#endif // NATIVE_BUILD
