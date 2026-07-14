#ifndef BUSTAFERL_INETWORK_H
#define BUSTAFERL_INETWORK_H

#include "NetInfo.h"
#include "ScanResult.h"

#include <cstdint>
#include <string>

#ifndef NATIVE_BUILD
#include <Stream.h>
#include <functional>
#endif

namespace bustaferl {

// Atomic result of an HTTP request. `ok` reflects whether the request
// completed at the transport level (no DNS/TLS/socket error); `http_status`
// carries the HTTP response code (0 on transport error). Callers must inspect
// both — `ok && http_status == 401` means "request went through, server said
// unauthenticated", which is the auth-tripwire path.
struct HttpResult {
  bool ok = false;
  int http_status = 0;
};

// Why the last connect() attempt failed. `AuthFailed` is terminal — the SSID
// was found and association started, but the WPA handshake failed (wrong
// password): retrying cannot fix it, so the caller stops and shows a dedicated
// screen. `NotFound` (no matching AP in range) and `None` (connected, or not
// yet attempted) keep the normal retry loop.
enum class WifiFailure : std::uint8_t {
  None,       // connected, or connect() not yet called
  NotFound,   // no configured SSID in range → keep retrying
  AuthFailed, // SSID found but WPA handshake failed (wrong password) → terminal
};

class INetwork {
public:
  virtual ~INetwork() = default;
  // Brings up WiFi with timeout in ms. Returns true if connected.
  virtual bool connect(unsigned timeout_ms) = 0;
  virtual bool isConnected() = 0;
  // GET, writes body to `out`. Returns HttpResult with transport flag + status.
  virtual HttpResult httpGet(const std::string &url, std::string &out) = 0;
  // POST, writes response body to `out`. Default content-type
  // "application/json".
  virtual HttpResult httpPost(const std::string &url, const std::string &body,
                              const std::string &content_type,
                              std::string &out) = 0;

  // Live association details for the diagnostic status line (SSID / IP / RSSI).
  // Defaulted to "no info" so host fakes and the native runtime — which never
  // associate — need no override. Esp32Network fills it from the WiFi driver.
  virtual NetInfo connectionInfo() { return NetInfo{}; }

  // Visible APs from the most recent scan (strongest first), for the
  // "KEIN EMPFANG" screen. Defaulted to empty so host fakes and the native
  // runtime need no override; Esp32Network reads its cached scan results.
  virtual ScanResult scanVisible() { return ScanResult{}; }

  // The SSIDs the device is configured to connect to, for the "KEIN EMPFANG"
  // screen's "gesucht:" line. Defaulted to empty; Esp32Network records them as
  // addAp() is called.
  virtual ConfiguredSsids configuredSsids() { return ConfiguredSsids{}; }

  // Why the most recent connect() failed. Lets the cold path tell a recoverable
  // "no AP in range" (keep retrying) apart from a terminal "wrong password"
  // (WPA handshake failed). Defaulted to None for host fakes.
  virtual WifiFailure lastFailure() { return WifiFailure::None; }

#ifndef NATIVE_BUILD
  // Streaming GET: invokes `consumer(stream)` with the response body
  // exposed as an Arduino `Stream`, without buffering the body in a
  // std::string first. Returns true iff the GET returned 2xx AND
  // `consumer` returned true. Only implemented on device — native fakes
  // don't see this method (use httpGet there).
  // `::Stream` is fully qualified because bustaferl defines its own
  // `Stream` enum (see StreamSnapshot.h).
  using StreamConsumer = std::function<bool(::Stream &)>;
  virtual bool httpGetStream(const std::string &url,
                             StreamConsumer consumer) = 0;
#endif
};

} // namespace bustaferl

#endif
