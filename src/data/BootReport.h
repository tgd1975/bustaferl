#ifndef BUSTAFERL_BOOTREPORT_H
#define BUSTAFERL_BOOTREPORT_H

#include "NetInfo.h" // WIFI_SSID_BUF / IPV4_STR_BUF
#include "StreamSnapshot.h"

#include <cstdint>
#include <ctime>

namespace bustaferl {

// Payload of the boot-check dashboard (CONCEPT.md §8): assembled by the cold
// cycle after the first fetch, rendered once and shown for BOOT_INFO_SHOW_S
// seconds before the regular first frame. Plain trivially-copyable data so it
// can ride inside RenderInput; `valid = false` (the default) makes
// OverlayKind::Boot fall back to the plain power-on splash.
struct BootReport {
  bool valid = false;

  // WLAN details (INetwork::connectionInfo). has_net_info=false renders the
  // WLAN row without SSID/RSSI/IP — the adapter could not report them.
  bool has_net_info = false;
  char ssid[WIFI_SSID_BUF] = "";
  char ip[IPV4_STR_BUF] = "";
  int rssi_dbm = 0;

  // Wall clock at report-build time; ntp_ok mirrors IClock::isSynced().
  time_t now = 0;
  bool ntp_ok = false;

  // First snapshot of this boot plus how its fetch went (FetchSummary).
  StreamSnapshot snap;
  bool oebb_http_ok = false;
  int batches_total = 0;
  int batches_failed = 0;
  int batches_retried = 0;

  // Morning-schedule hints: bus streams with hints loaded vs. expected.
  bool schedule_ok = false;
  int hint_streams_loaded = 0;
  int hint_streams_expected = 0;

  // What survived in RTC RAM from before this boot (all false after a real
  // power loss; true after a software reset / brown-out with RTC intact).
  bool meta_restored = false;
  bool frame_restored = false;
  bool schedule_restored = false;

  // WLAN+NTP came up on attempt `boot_attempt` of `boot_attempts_max`.
  int boot_attempt = 1;
  int boot_attempts_max = 1;

  // Milliseconds since power-on (IClock::ticksMs; 0 = unknown).
  std::uint32_t uptime_ms = 0;

  // Heap state, filled by Esp32Renderer right before drawing (0 = unknown —
  // the platform-neutral cycle cannot probe the ESP32 heap).
  std::uint32_t heap_free_bytes = 0;
  std::uint32_t heap_largest_bytes = 0;

  // How long the dashboard stays before the regular frame replaces it.
  int show_s = 0;
};

} // namespace bustaferl

#endif
