#ifndef BUSTAFERL_INETWORK_H
#define BUSTAFERL_INETWORK_H

#include <string>

namespace bustaferl {

class INetwork {
public:
  virtual ~INetwork() = default;
  // Brings up WiFi with timeout in ms. Returns true if connected.
  virtual bool connect(unsigned timeout_ms) = 0;
  virtual bool isConnected() = 0;
  // GET, writes body to `out`. Returns true on HTTP 2xx.
  virtual bool httpGet(const std::string &url, std::string &out) = 0;
};

} // namespace bustaferl

#endif
