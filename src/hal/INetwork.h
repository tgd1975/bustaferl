#ifndef BUSTAFERL_INETWORK_H
#define BUSTAFERL_INETWORK_H

#include <string>

#ifndef NATIVE_BUILD
#include <Stream.h>
#include <functional>
#endif

namespace bustaferl {

class INetwork {
public:
  virtual ~INetwork() = default;
  // Brings up WiFi with timeout in ms. Returns true if connected.
  virtual bool connect(unsigned timeout_ms) = 0;
  virtual bool isConnected() = 0;
  // GET, writes body to `out`. Returns true on HTTP 2xx.
  virtual bool httpGet(const std::string &url, std::string &out) = 0;

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
