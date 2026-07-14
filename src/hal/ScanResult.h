#ifndef BUSTAFERL_HAL_SCANRESULT_H
#define BUSTAFERL_HAL_SCANRESULT_H

#include "NetInfo.h" // NET_SSID_BUF

#include <cstdint>

namespace bustaferl {

// Snapshot of the APs a scan saw, for the "KEIN EMPFANG" screen: when no
// configured network matched, show which SSIDs *were* visible so a field
// diagnosis can tell whether the home AP is absent, renamed, or on an
// out-of-range channel. Leaf struct (no Arduino, no heap) so the device
// adapter and the host build share it; a fixed cap keeps it RTC/stack-cheap.
constexpr int SCAN_MAX_APS = 6;

struct ScanEntry {
  char ssid[NET_SSID_BUF] = "";
  std::int8_t rssi_dbm = 0;
  std::uint8_t channel = 0;
};

struct ScanResult {
  int count = 0; // number of populated `aps` entries (0..SCAN_MAX_APS)
  ScanEntry aps[SCAN_MAX_APS];
};

// The SSIDs the device is *configured to look for* (primary + optional
// secondary), so the "KEIN EMPFANG" screen can show "gesucht: <name>" next to
// the list of networks actually in range — the two together make the mismatch
// obvious (wrong name, out of range, AP down). Names only; no passwords.
constexpr int MAX_CONFIGURED_APS = 2;

struct ConfiguredSsids {
  int count = 0; // number of populated `ssid` entries (0..MAX_CONFIGURED_APS)
  char ssid[MAX_CONFIGURED_APS][NET_SSID_BUF] = {};
};

// A visible network whose SSID matches a *configured* one only when case is
// ignored — the AP the device wants is in range, but WiFiMulti's case-sensitive
// comparison rejected it (802.11 SSIDs are opaque octet strings, so "A-NET2" !=
// "a-net2"). The field-observed footgun: the router broadcasts a lowercase SSID
// while config has it capitalised, and the board reports "no matching wifi
// found" despite the AP sitting there at full signal. Surfacing it turns a
// silent typo into an obvious "did you mean?" in the log and on-screen.
// (Data lives here so the renderer can carry it without a render→logic dep; the
// matching logic is logic/ssid_match.{h,cpp}.)
struct SsidCaseMismatch {
  bool found = false;
  char configured[NET_SSID_BUF] = ""; // what config asked for
  char visible[NET_SSID_BUF] = "";    // what the beacon actually broadcasts
};

} // namespace bustaferl

#endif
