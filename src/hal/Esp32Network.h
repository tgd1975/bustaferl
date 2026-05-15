#ifndef BUSTAFERL_ESP32NETWORK_H
#define BUSTAFERL_ESP32NETWORK_H

#ifndef NATIVE_BUILD

#include <WiFiMulti.h>

#include "INetwork.h"

namespace bustaferl {

class Esp32Network : public INetwork {
public:
  void addAp(const char *ssid, const char *password);
  bool connect(unsigned timeout_ms) override;
  bool isConnected() override;
  bool httpGet(const std::string &url, std::string &out) override;

private:
  WiFiMulti wifi_;
};

} // namespace bustaferl

#endif
#endif
