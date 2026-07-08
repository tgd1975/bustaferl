#ifndef BUSTAFERL_NETINFO_H
#define BUSTAFERL_NETINFO_H

namespace bustaferl {

// Buffer sizes for WiFi connection details: 802.11 SSID max 32 bytes + NUL;
// dotted IPv4 + NUL.
constexpr int WIFI_SSID_BUF = 33;
constexpr int IPV4_STR_BUF = 16;

// Connection details for the boot-check dashboard. Fixed-size buffers keep
// the struct trivially copyable (it feeds BootReport, which rides in
// RenderInput). Deliberately a leaf header with zero includes: INetwork.h
// pulls it in, and anything heavier (StreamSnapshot's `Stream` enum!) would
// change unqualified-name lookup inside the ESP32 network adapter, which
// derives helper classes from Arduino's global ::Stream.
struct NetInfo {
  char ssid[WIFI_SSID_BUF] = "";
  char ip[IPV4_STR_BUF] = "";
  int rssi_dbm = 0;
};

} // namespace bustaferl

#endif
