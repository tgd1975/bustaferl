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

// Snprintf buffer sizes for the per-state foot/sub strings.
constexpr std::size_t SUB_BUF_CAP = 48;
constexpr std::size_t FOOT_BUF_CAP = 48;

void drawCentered(render::Canvas &canvas, FontRole role, int y,
                  const char *text) {
  canvas.setRoleFont(role);
  canvas.setTextColor(1);
  const int w = canvas.textWidth(text);
  canvas.setCursor((FB_W - w) / 2, y);
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
  drawCentered(canvas, FontRole::Fullscreen_Title, TITLE_Y, "BUSTAFERL");
  // U+2026 (…) ist in u8g2_font_helvR14_te nicht enthalten (Coverage-Check
  // 2026-05-20 ergab leeres Glyph). Drei ASCII-Punkte bleiben als sichtbarer
  // Fallback; Drift gegenüber Design-PNG ist 1-px-kosmetisch.
  drawCentered(canvas, FontRole::Fullscreen_Sub, SUB_Y, "lädt Fahrplan...");
  if (version_str != nullptr) {
    drawCentered(canvas, FontRole::Fullscreen_Foot, FOOT_Y, version_str);
  }
}

// The KEIN-EMPFANG diagnostic block sits between the sub line and the footer:
// one "gesucht:" line naming the configured SSIDs (or a case-mismatch "did you
// mean?" hint when one was detected), then the list of networks actually in
// range. Y anchors are hand-placed to clear FOOT_Y with 3 rows.
constexpr int OFFLINE_WANTED_Y = SUB_Y + 20;
constexpr int OFFLINE_FOUND_Y0 = OFFLINE_WANTED_Y + 16;
constexpr int OFFLINE_SSID_ROWS = 3;
constexpr int OFFLINE_SSID_ROW_H = 12;

// When a configured SSID matched a visible one only ignoring case, that IS the
// diagnosis — show both spellings instead of the plain "gesucht:" line. Returns
// true if it drew the hint (caller then skips drawWantedSsids).
bool drawCaseMismatchHint(render::Canvas &canvas,
                          const SsidCaseMismatch &mismatch) {
  if (!mismatch.found) {
    return false;
  }
  // Worst case: fixed text (~19) + two full-length SSIDs (NET_SSID_BUF each).
  char line[24 + 2 * NET_SSID_BUF];
  std::snprintf(line, sizeof(line), "Schreibweise? \"%s\" vs \"%s\"",
                mismatch.configured, mismatch.visible);
  drawCentered(canvas, FontRole::Fullscreen_Foot, OFFLINE_WANTED_Y, line);
  return true;
}

// "gesucht: A-NET2" (or "gesucht: A-NET2, Fallback") — the SSIDs the device is
// configured to look for. Skipped when none were configured.
void drawWantedSsids(render::Canvas &canvas, const ConfiguredSsids &wanted) {
  if (wanted.count <= 0) {
    return;
  }
  char line[SUB_BUF_CAP];
  int n = std::snprintf(line, sizeof(line), "gesucht: %s", wanted.ssid[0]);
  for (int i = 1; i < wanted.count && n > 0 && n < (int)sizeof(line); ++i) {
    n += std::snprintf(line + n, sizeof(line) - n, ", %s", wanted.ssid[i]);
  }
  drawCentered(canvas, FontRole::Fullscreen_Foot, OFFLINE_WANTED_Y, line);
}

void drawVisibleAps(render::Canvas &canvas, const ScanResult &aps) {
  const int rows =
      aps.count < OFFLINE_SSID_ROWS ? aps.count : OFFLINE_SSID_ROWS;
  if (rows <= 0) {
    drawCentered(canvas, FontRole::Fullscreen_Foot, OFFLINE_FOUND_Y0,
                 "Keine Netze in Reichweite");
    return;
  }
  drawCentered(canvas, FontRole::Fullscreen_Foot, OFFLINE_FOUND_Y0,
               "Gefundene Netze:");
  for (int i = 0; i < rows; ++i) {
    char line[SUB_BUF_CAP];
    std::snprintf(line, sizeof(line), "%s  %ddBm", aps.aps[i].ssid,
                  aps.aps[i].rssi_dbm);
    drawCentered(canvas, FontRole::Fullscreen_Foot,
                 OFFLINE_FOUND_Y0 + (i + 1) * OFFLINE_SSID_ROW_H, line);
  }
}

void drawOffline(render::Canvas &canvas, std::time_t last_fetch_at,
                 int retry_in_s, const ScanResult &visible_aps,
                 const ConfiguredSsids &wanted_ssids,
                 const SsidCaseMismatch &case_mismatch) {
  drawCustomGlyph(canvas, GLYPH_X, GLYPH_Y,
                  GlyphBitmap{GLYPH_EXCLAMATION_90, GLYPH_W, GLYPH_H});
  drawCentered(canvas, FontRole::Fullscreen_Title, TITLE_Y, "KEIN EMPFANG");

  char sub_buf[SUB_BUF_CAP] = "Noch nie aktualisiert";
  if (last_fetch_at > 0) {
    char hhmm[8];
    formatHHMM(last_fetch_at, hhmm, sizeof(hhmm));
    std::snprintf(sub_buf, sizeof(sub_buf), "Letzte Aktualisierung %s", hhmm);
  }
  drawCentered(canvas, FontRole::Fullscreen_Sub, SUB_Y, sub_buf);

  // A case-only mismatch is the actionable diagnosis; it replaces the plain
  // "gesucht:" line when present.
  if (!drawCaseMismatchHint(canvas, case_mismatch)) {
    drawWantedSsids(canvas, wanted_ssids);
  }
  drawVisibleAps(canvas, visible_aps);

  char foot_buf[SUB_BUF_CAP];
  if (retry_in_s > 0) {
    std::snprintf(foot_buf, sizeof(foot_buf), "WLAN · Retry in %ds",
                  retry_in_s);
  } else {
    std::snprintf(foot_buf, sizeof(foot_buf), "WLAN · retrying...");
  }
  drawCentered(canvas, FontRole::Fullscreen_Foot, FOOT_Y, foot_buf);
}

void drawAuth(render::Canvas &canvas, const char *aid_short, int http_code) {
  drawCustomGlyph(canvas, GLYPH_X, GLYPH_Y,
                  GlyphBitmap{GLYPH_PARA_NINE_90, GLYPH_W, GLYPH_H});
  drawCentered(canvas, FontRole::Fullscreen_Title, TITLE_Y, "AUTH-FEHLER");
  drawCentered(canvas, FontRole::Fullscreen_Sub, SUB_Y,
               "Client-ID veraltet · bitte neu registrieren");

  char foot_buf[FOOT_BUF_CAP];
  const char *aid =
      (aid_short != nullptr && aid_short[0] != '\0') ? aid_short : "---";
  if (http_code > 0) {
    std::snprintf(foot_buf, sizeof(foot_buf), "AID %s · ERR %d", aid,
                  http_code);
  } else {
    std::snprintf(foot_buf, sizeof(foot_buf), "AID %s · ERR ---", aid);
  }
  drawCentered(canvas, FontRole::Fullscreen_Foot, FOOT_Y, foot_buf);
}

void drawWifiAuth(render::Canvas &canvas, const ConfiguredSsids &wanted_ssids) {
  drawCustomGlyph(canvas, GLYPH_X, GLYPH_Y,
                  GlyphBitmap{GLYPH_EXCLAMATION_90, GLYPH_W, GLYPH_H});
  drawCentered(canvas, FontRole::Fullscreen_Title, TITLE_Y, "WLAN-PASSWORT");

  // Name the network whose handshake failed so the fix is unambiguous. Sized
  // for the fixed text (~26) plus a full-length SSID (NET_SSID_BUF).
  char sub_buf[32 + NET_SSID_BUF];
  if (wanted_ssids.count > 0) {
    std::snprintf(sub_buf, sizeof(sub_buf), "Falsches Passwort für \"%s\"",
                  wanted_ssids.ssid[0]);
  } else {
    std::snprintf(sub_buf, sizeof(sub_buf), "Falsches WLAN-Passwort");
  }
  drawCentered(canvas, FontRole::Fullscreen_Sub, SUB_Y, sub_buf);

  // Terminal state — say so, so the user knows waiting won't help.
  drawCentered(canvas, FontRole::Fullscreen_Foot, FOOT_Y,
               "Passwort in secrets.h korrigieren · kein Retry");
}

void drawQuiet(render::Canvas &canvas) {
  // Em-dash bar — the design uses a 72 px-wide VT323 em-dash glyph. The
  // bitmap fonts have no glyph that wide, so we draw a solid paper-coloured
  // bar with the same visual weight. Roughly centred horizontally and
  // vertically against the 90 px glyph region the other fullscreen states
  // use, so the section structure feels consistent.
  constexpr int QUIET_BAR_W = 72;
  constexpr int QUIET_BAR_H = 10;
  const int bar_x = (FB_W - QUIET_BAR_W) / 2;
  const int bar_y = GLYPH_Y + (GLYPH_H - QUIET_BAR_H) / 2;
  canvas.fillRect(bar_x, bar_y, QUIET_BAR_W, QUIET_BAR_H, 1);
  drawCentered(canvas, FontRole::Fullscreen_Title, TITLE_Y, "KEINE ABFAHRTEN");
  drawCentered(canvas, FontRole::Fullscreen_Sub, SUB_Y,
               "in den nächsten 20 min");
}

} // namespace bustaferl
