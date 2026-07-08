#ifndef BUSTAFERL_ESP32NETWORK_H
#define BUSTAFERL_ESP32NETWORK_H

#ifndef NATIVE_BUILD

#include "INetwork.h"

#include <WiFiMulti.h>

namespace bustaferl {

class Esp32Network : public INetwork {
public:
  void addAp(const char *ssid, const char *password);
  bool connect(unsigned timeout_ms) override;
  bool isConnected() override;
  bool connectionInfo(NetInfo &out) override;
  bool httpGet(const std::string &url, std::string &out) override;
  bool httpPost(const std::string &url, const std::string &body,
                const std::string &content_type, std::string &out) override;
  bool httpGetStream(const std::string &url, StreamConsumer consumer) override;

private:
  WiFiMulti wifi_;
};

} // namespace bustaferl

#endif
#endif
