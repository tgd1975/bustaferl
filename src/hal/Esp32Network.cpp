#include "Esp32Network.h"

#ifndef NATIVE_BUILD

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

namespace bustaferl {

void Esp32Network::addAp(const char *ssid, const char *password) {
  wifi_.addAP(ssid, password);
}

bool Esp32Network::connect(unsigned timeout_ms) {
  return wifi_.run(timeout_ms) == WL_CONNECTED;
}

bool Esp32Network::isConnected() { return WiFi.status() == WL_CONNECTED; }

bool Esp32Network::httpGet(const std::string &url, std::string &out) {
  out.clear();
  Serial.printf("[net] GET %s\n", url.c_str());
  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure(); // OGD has Let's Encrypt; we don't ship the CA bundle
  if (!http.begin(client, url.c_str())) {
    Serial.println("[net] http.begin() failed");
    return false;
  }
  http.setTimeout(8000);
  int code = http.GET();
  if (code < 200 || code >= 300) {
    Serial.printf("[net] HTTP %d (non-2xx, aborting)\n", code);
    http.end();
    return false;
  }
  out = http.getString().c_str();
  Serial.printf("[net] HTTP %d, %u bytes\n", code,
                static_cast<unsigned>(out.size()));
  http.end();
  return true;
}

} // namespace bustaferl

#endif
