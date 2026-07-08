#ifndef BUSTAFERL_INETWORK_H
#define BUSTAFERL_INETWORK_H

#include "../data/BootReport.h" // WIFI_SSID_BUF / IPV4_STR_BUF

#include <string>

#ifndef NATIVE_BUILD
#include <Stream.h>
#include <functional>
#endif

namespace bustaferl {

// Connection details for the boot-check dashboard. Fixed-size buffers keep
// the struct trivially copyable (it feeds BootReport, which rides in
// RenderInput).
struct NetInfo {
  char ssid[WIFI_SSID_BUF] = "";
  char ip[IPV4_STR_BUF] = "";
  int rssi_dbm = 0;
};

class INetwork {
public:
  virtual ~INetwork() = default;
  // Brings up WiFi with timeout in ms. Returns true if connected.
  virtual bool connect(unsigned timeout_ms) = 0;
  virtual bool isConnected() = 0;
  // Fills `out` with details of the current connection. Default: not
  // available (host fakes, curl runtime) — the dashboard then omits them.
  virtual bool connectionInfo(NetInfo &out) {
    (void)out;
    return false;
  }
  // GET, writes body to `out`. Returns true on HTTP 2xx.
  virtual bool httpGet(const std::string &url, std::string &out) = 0;
  // POST `body` with `content_type`, writes response body to `out`. Returns
  // true on HTTP 2xx. Used by the ÖBB HAFAS (mgate.exe) S-Bahn fetch; the
  // response (~5–8 KB) is small enough that no streaming variant is needed.
  virtual bool httpPost(const std::string &url, const std::string &body,
                        const std::string &content_type, std::string &out) = 0;

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
