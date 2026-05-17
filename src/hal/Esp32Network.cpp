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
  // Release dangling capacity from a previous body — otherwise a 37 KB EFA
  // response stays allocated across the next iteration even though the
  // visible string was assigned to.
  std::string().swap(out);
  Serial.printf("[net] GET %s heap_free=%u largest=%u\n", url.c_str(),
                static_cast<unsigned>(ESP.getFreeHeap()),
                static_cast<unsigned>(
                    heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT)));
  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure(); // OGD/EFA Let's Encrypt; CA bundle not shipped
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
  // getString() is the known-good code path: returns an Arduino String with
  // a single internal allocation. Tear down the HTTPClient immediately so
  // its internal 37 KB buffer is freed before we assign into `out` —
  // otherwise the peak is three concurrent copies (HTTPClient + tmp + out).
  String tmp = http.getString();
  http.end();
  out.assign(tmp.c_str(), tmp.length());
  // tmp is destroyed when this scope ends; peak after http.end() is two
  // copies (tmp + out) for the duration of the assign.
  Serial.printf("[net] HTTP %d, %u bytes\n", code,
                static_cast<unsigned>(out.size()));
  return true;
}

} // namespace bustaferl

#endif
