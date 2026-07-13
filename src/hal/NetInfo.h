#ifndef BUSTAFERL_HAL_NETINFO_H
#define BUSTAFERL_HAL_NETINFO_H

namespace bustaferl {

// Live WiFi association details for the diagnostic / boot-check status line.
// Leaf struct (no Arduino, no other bustaferl deps) so the device adapter and
// the host build both share it. Buffers mirror DiagView's DIAG_SSID_BUF /
// DIAG_IP_BUF: 802.11 SSID max 32 bytes + NUL, dotted IPv4 + NUL.
constexpr int NET_SSID_BUF = 33;
constexpr int NET_IP_BUF = 16;

struct NetInfo {
  bool valid = false;
  char ssid[NET_SSID_BUF] = "";
  char ip[NET_IP_BUF] = "";
  int rssi_dbm = 0;
};

} // namespace bustaferl

#endif
