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
  HttpResult httpGet(const std::string &url, std::string &out) override;
  HttpResult httpPost(const std::string &url, const std::string &body,
                      const std::string &content_type,
                      std::string &out) override;
  NetInfo connectionInfo() override;
  ScanResult scanVisible() override;
  ConfiguredSsids configuredSsids() override;
  WifiFailure lastFailure() override;
  bool httpGetStream(const std::string &url, StreamConsumer consumer) override;

private:
  WiFiMulti wifi_;
  // Names passed to addAp(), for the "KEIN EMPFANG" screen's "gesucht:" line.
  ConfiguredSsids configured_;
  // Classified result of the last connect() (auth vs. not-found), for the
  // terminal wrong-password screen.
  WifiFailure last_failure_ = WifiFailure::None;
  // WiFi.onEvent handler registered only once (first connect()).
  bool event_registered_ = false;
};

} // namespace bustaferl

#endif
#endif
