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
  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure(); // OGD has Let's Encrypt; we don't ship the CA bundle
  if (!http.begin(client, url.c_str()))
    return false;
  http.setTimeout(8000);
  int code = http.GET();
  if (code < 200 || code >= 300) {
    http.end();
    return false;
  }
  out = http.getString().c_str();
  http.end();
  return true;
}

} // namespace bustaferl

#endif
