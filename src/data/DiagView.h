#ifndef BUSTAFERL_DATA_DIAGVIEW_H
#define BUSTAFERL_DATA_DIAGVIEW_H

#include "CycleTrace.h"
#include "ScheduleHint.h"
#include "StreamSnapshot.h"

#include <cstdint>
#include <ctime>

namespace bustaferl {

// Buffer sizes for the WiFi connection details shown on the diagnostic and
// boot-check screens: 802.11 SSID max 32 bytes + NUL, dotted IPv4 + NUL.
constexpr int DIAG_SSID_BUF = 33;
constexpr int DIAG_IP_BUF = 16;

// Everything the diagnostic pages (double-click) and the boot-check screen
// render. Built transiently when entering the mode — NOT persisted, so its
// size does not touch the RTC budget. Plain data so the render layer stays
// host-testable.
struct DiagView {
  // --- Live status (STATUS page + boot-check) ---
  bool has_net_info = false;
  char ssid[DIAG_SSID_BUF] = "";
  char ip[DIAG_IP_BUF] = "";
  int rssi_dbm = 0;
  time_t now = 0;
  bool ntp_ok = false;
  time_t last_ntp_sync = 0;
  StreamSnapshot snap;       // per-stream self-test (STATUS / DATA)
  ScheduleSnapshot schedule; // hints (DATA page)
  bool stale = false;
  std::uint8_t filter_miss_streak = 0;
  std::uint8_t ogd_auth_streak = 0;
  bool auth_error_seen = false;
  std::uint32_t heap_free_kb = 0;
  std::uint32_t heap_largest_kb = 0;
  std::uint32_t uptime_s = 0;

  // --- Panel state (DATA page) ---
  std::uint16_t partial_count = 0;
  time_t last_light_full = 0;
  time_t last_deep_clean = 0;

  // --- Boot-check extras ---
  int boot_attempt = 1;
  int boot_attempts_max = 1;
  bool meta_restored = false;
  bool frame_restored = false;
  bool schedule_restored = false;
  int batches_total = 0;
  int batches_failed = 0;
  int batches_retried = 0;
  bool oebb_http_ok = false;
  int show_s = 0;                // boot-check countdown (0 hides the line)
  const char *version = nullptr; // firmware version string
  int diag_page = 0;             // current DiagPage (footer hint)

  // --- History (CYCLES / ERRORS pages) ---
  CycleTrace trace;
};

} // namespace bustaferl

#endif
