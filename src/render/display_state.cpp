#include "render/display_state.h"

#include "render/custom_glyph.h"
#include "render/glyph_dotted_circle_90.h"
#include "render/glyph_exclamation_90.h"
#include "render/glyph_para_nine_90.h"
#include "render/layout.h" // FB_W / FB_H

#include <cstdio>
#include <cstring>
#include <ctime>

namespace bustaferl {

namespace {

constexpr int GLYPH_W = 90;
constexpr int GLYPH_H = 90;
constexpr int GLYPH_X = (FB_W - GLYPH_W) / 2;
constexpr int GLYPH_Y = 64;

constexpr int TITLE_Y = GLYPH_Y + GLYPH_H + 24;
constexpr int SUB_Y = TITLE_Y + 28;
constexpr int FOOT_Y = FB_H - 12;

// Crude horizontal-centring constant — pixels per ASCII char. U8g2's
// getUTF8Width would be exact; "close enough" is the apprentice contract.
constexpr int CENTRE_PX_PER_CHAR = 6;

// Snprintf buffer sizes for the per-state foot/sub strings.
constexpr std::size_t SUB_BUF_CAP = 40;
constexpr std::size_t FOOT_BUF_CAP = 48;

void drawCentered(render::Canvas &canvas, FontRole role, int y,
                  const char *text) {
  canvas.setRoleFont(role);
  canvas.setTextColor(1);
  int est_w = CENTRE_PX_PER_CHAR * static_cast<int>(std::strlen(text));
  canvas.setCursor((FB_W - est_w) / 2, y);
  canvas.print(text);
}

// Minimum buffer size to format "HH:MM\0".
constexpr std::size_t HHMM_BUF_MIN = 6;

void formatHHMM(std::time_t t, char *out, std::size_t cap) {
  if (cap < HHMM_BUF_MIN) {
    return;
  }
  struct tm local{};
  localtime_r(&t, &local);
  std::snprintf(out, cap, "%02d:%02d", local.tm_hour, local.tm_min);
}

} // namespace

void drawBoot(render::Canvas &canvas, const char *version_str) {
  drawCustomGlyph(canvas, GLYPH_X, GLYPH_Y,
                  GlyphBitmap{GLYPH_DOTTED_CIRCLE_90, GLYPH_W, GLYPH_H});
  drawCentered(canvas, FontRole::Fullscreen_Title, TITLE_Y, "bustaferl");
  drawCentered(canvas, FontRole::Fullscreen_Sub, SUB_Y, "laedt Fahrplan...");
  if (version_str != nullptr) {
    drawCentered(canvas, FontRole::Fullscreen_Foot, FOOT_Y, version_str);
  }
}

void drawOffline(render::Canvas &canvas, std::time_t last_fetch_at,
                 int retry_in_s) {
  drawCustomGlyph(canvas, GLYPH_X, GLYPH_Y,
                  GlyphBitmap{GLYPH_EXCLAMATION_90, GLYPH_W, GLYPH_H});
  drawCentered(canvas, FontRole::Fullscreen_Title, TITLE_Y, "Kein Empfang");

  char sub_buf[SUB_BUF_CAP] = "Noch nie aktualisiert";
  if (last_fetch_at > 0) {
    char hhmm[8];
    formatHHMM(last_fetch_at, hhmm, sizeof(hhmm));
    std::snprintf(sub_buf, sizeof(sub_buf), "Letzte Aktualisierung %s", hhmm);
  }
  drawCentered(canvas, FontRole::Fullscreen_Sub, SUB_Y, sub_buf);

  char foot_buf[SUB_BUF_CAP];
  if (retry_in_s > 0) {
    std::snprintf(foot_buf, sizeof(foot_buf), "WLAN - Retry in %ds",
                  retry_in_s);
  } else {
    std::snprintf(foot_buf, sizeof(foot_buf), "WLAN - retrying...");
  }
  drawCentered(canvas, FontRole::Fullscreen_Foot, FOOT_Y, foot_buf);
}

void drawAuth(render::Canvas &canvas, const char *aid_short, int http_code) {
  drawCustomGlyph(canvas, GLYPH_X, GLYPH_Y,
                  GlyphBitmap{GLYPH_PARA_NINE_90, GLYPH_W, GLYPH_H});
  drawCentered(canvas, FontRole::Fullscreen_Title, TITLE_Y, "Auth-Fehler");
  drawCentered(canvas, FontRole::Fullscreen_Sub, SUB_Y,
               "Client-ID veraltet - bitte neu registrieren");

  char foot_buf[FOOT_BUF_CAP];
  const char *aid =
      (aid_short != nullptr && aid_short[0] != '\0') ? aid_short : "AID:---";
  if (http_code > 0) {
    std::snprintf(foot_buf, sizeof(foot_buf), "%s - ERR %d", aid, http_code);
  } else {
    std::snprintf(foot_buf, sizeof(foot_buf), "%s - ERR ---", aid);
  }
  drawCentered(canvas, FontRole::Fullscreen_Foot, FOOT_Y, foot_buf);
}

void drawQuiet(render::Canvas &canvas) {
  // Heavy "---" substitute for the 72 px dash from the design handoff.
  constexpr int QUIET_DASH_X_OFFSET = 30;
  constexpr int QUIET_DASH_Y_OFFSET = 12;
  canvas.setRoleFont(FontRole::Fullscreen_Title);
  canvas.setTextColor(1);
  canvas.setCursor(FB_W / 2 - QUIET_DASH_X_OFFSET,
                   GLYPH_Y + GLYPH_H / 2 + QUIET_DASH_Y_OFFSET);
  canvas.print("---");
  drawCentered(canvas, FontRole::Fullscreen_Title, TITLE_Y, "Keine Abfahrten");
  drawCentered(canvas, FontRole::Fullscreen_Sub, SUB_Y,
               "in den naechsten 20 min");
}

} // namespace bustaferl
