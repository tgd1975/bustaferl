#ifndef BUSTAFERL_LAYOUT_H
#define BUSTAFERL_LAYOUT_H

#include "../data/StreamSnapshot.h"
#include "../hal/ScanResult.h"
#include "frame_buffer.h"

#include <cstdint>
#include <ctime>

namespace bustaferl {

constexpr int FB_W = 400;
constexpr int FB_H = 300;
using Frame = FrameBuffer<FB_W, FB_H>;

// Display modes the state-selector decides between. Only error / placeholder
// screens are their own state; every screen that shows departures is Normal,
// where the merged data decides what appears (fresh, scheduled-only, stale, or
// a quiet gap all render the same board). Stale / Night / Quiet were removed —
// they discarded real departure times for a blank or redundant screen.
enum class DisplayState : std::uint8_t {
  Boot,     // first render ever (post-MAGIC-bump too) — "lädt Fahrplan…"
  Normal,   // the departure board: realtime + schedule hints, full layout
  Offline,  // wifi down AND last_success older than OFFLINE_THRESHOLD_S
  Auth,     // HAFAS err=AID/AUTH OR three consecutive OGD 401/403
  WifiAuth, // WPA handshake failed (wrong WiFi password) — terminal
};

// Short AID prefix carried into the Auth fullscreen renderer. 8 hex chars
// plus terminator. Kept zero-initialised when not in Auth state.
constexpr int AUTH_AID_SHORT_CAP = 10;

struct RenderInput {
  DisplayState state = DisplayState::Normal;

  // Data layer — populated for Normal; empty otherwise.
  StreamSnapshot snapshot;

  // Offline-state inputs.
  std::time_t last_fetch_at = 0;
  int retry_in_s = 0;
  // Visible APs when no configured network matched — shown on "KEIN EMPFANG"
  // so the user sees which SSIDs are in range. Empty (count 0) → no list.
  ScanResult visible_aps;
  // The SSIDs the device is looking for — shown as a "gesucht:" line so the
  // configured-vs-visible mismatch is obvious. Empty (count 0) → no line.
  ConfiguredSsids wanted_ssids;
  // A configured SSID that matched a visible one only ignoring case — shown as
  // a prominent "did you mean?" hint. found=false → not shown.
  SsidCaseMismatch case_mismatch;

  // Auth-state inputs.
  char auth_aid_short[AUTH_AID_SHORT_CAP] = {0};
  int auth_http_code = 0;

  // Boot-state input. Pointer to DISPLAY_VERSION_STR; renderer never frees.
  const char *firmware_version = nullptr;
};

// Renders the layout described in CONCEPT.md §3 into the framebuffer.
// Picks the right Canvas implementation per target:
//   - ESP32: AdafruitGfxCanvas (master — Adafruit_GFX + U8g2).
//   - Native host: HostCanvas (apprentice — builtin 5×7 font, functional
//     testing only, not pixel-identical).
// Both targets exercise the same render/* code.
void renderFrame(const RenderInput &in, Frame &fb);

// Debug stamp (UPDATE_STAMP_ENABLED): overdraws a small "upd HH:MM" bottom-
// right of an already-rendered frame — the time the panel content last
// actually changed. Clears its own background, so overwriting an older stamp
// is safe. t == 0 draws nothing. Called by cycle_runner after the refresh
// decision, never from renderFrame (mockups and baselines stay stamp-free).
void drawUpdateStamp(Frame &fb, std::time_t t);

} // namespace bustaferl

#endif
