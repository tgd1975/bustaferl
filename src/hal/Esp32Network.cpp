#include "Esp32Network.h"

#ifndef NATIVE_BUILD

#include "../logic/ssid_match.h"

#include <HTTPClient.h>
#include <Stream.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <cstdio>
#include <esp_wifi.h>

namespace bustaferl {

namespace {

// Up-front size for a buffered POST response body whose length the server
// does not announce. Sized above the largest observed HAFAS mgate.exe reply
// (~30 KB and slowly growing over the day) so one allocation covers the whole
// body. Only the buffered httpPost() path needs it; httpPostStream() never
// materialises the body at all.
constexpr std::size_t POST_RESPONSE_RESERVE_BYTES = 48U * 1024U;

// Append-only Stream that forwards into a std::string. Lets us pass `out`
// to HTTPClient::writeToStream() so the body lands directly in its final
// buffer — no Arduino String intermediate, so no 2x 37 KB peak (which on
// the third back-to-back EFA call fragmented the heap and threw bad_alloc
// inside assign()). HTTPClient still handles chunked Transfer-Encoding.
class StringAppender : public Stream {
public:
  explicit StringAppender(std::string &s) : s_(s) {}
  size_t write(uint8_t b) override {
    s_.push_back(static_cast<char>(b));
    return 1;
  }
  size_t write(const uint8_t *buf, size_t size) override {
    s_.append(reinterpret_cast<const char *>(buf), size);
    return size;
  }
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }

private:
  std::string &s_;
};

// Blocking byte fetch from a WiFiClient. Waits up to timeout_ms for the
// next byte. ArduinoJson's deserializer is pull-based and calls read()
// byte-by-byte; the raw WiFiClient returns -1 immediately when its TCP
// buffer is empty, which on a 38 KB body arriving over multiple TCP
// segments would abort the parse halfway.
int waitForByte(WiFiClient &c, uint32_t timeout_ms, bool consume) {
  uint32_t start = millis();
  for (;;) {
    if (c.available()) {
      return consume ? c.read() : c.peek();
    }
    if (!c.connected() && !c.available()) {
      return -1;
    }
    if (millis() - start > timeout_ms) {
      return -1;
    }
    delay(1);
  }
}

// Pass-through blocking stream used when the response has a Content-Length
// (identity transfer encoding).
class BlockingClientStream : public Stream {
public:
  BlockingClientStream(WiFiClient &c, uint32_t timeout_ms)
      : c_(c), timeout_ms_(timeout_ms) {}
  int available() override { return c_.available(); }
  int read() override { return waitForByte(c_, timeout_ms_, true); }
  int peek() override { return waitForByte(c_, timeout_ms_, false); }
  size_t write(uint8_t) override { return 0; }

private:
  WiFiClient &c_;
  uint32_t timeout_ms_;
};

// Chunked Transfer-Encoding decoder. EFA's XSLT_DM_REQUEST endpoint
// answers without Content-Length and uses chunked framing
// (`<hex-size>\r\n<bytes>\r\n…\r\n0\r\n\r\n`). HTTPClient's writeToStream
// would strip these markers, but we read from the raw WiFiClient to keep
// the body out of memory — so we strip them ourselves and present only
// the decoded body bytes to the consumer.
class ChunkedDecodingStream : public Stream {
public:
  ChunkedDecodingStream(WiFiClient &c, uint32_t timeout_ms)
      : c_(c), timeout_ms_(timeout_ms), remaining_(0), done_(false) {}

  int available() override {
    if (done_) {
      return 0;
    }
    if (remaining_ > 0) {
      int a = c_.available();
      return a < remaining_ ? a : remaining_;
    }
    return 0;
  }

  int read() override {
    if (!ensureChunk()) {
      return -1;
    }
    int b = waitForByte(c_, timeout_ms_, true);
    if (b < 0) {
      done_ = true;
      return -1;
    }
    --remaining_;
    if (remaining_ == 0) {
      // Consume the trailing \r\n after the chunk data so the next
      // ensureChunk() lands on the next size line.
      waitForByte(c_, timeout_ms_, true);
      waitForByte(c_, timeout_ms_, true);
    }
    return b;
  }

  int peek() override {
    if (!ensureChunk()) {
      return -1;
    }
    return waitForByte(c_, timeout_ms_, false);
  }

  size_t write(uint8_t) override { return 0; }

private:
  // Make sure we're positioned at the next body byte (i.e. remaining_ > 0
  // and the current byte isn't a chunk-size header). Returns false on
  // EOF (last chunk consumed) or stream error.
  bool ensureChunk() {
    if (done_) {
      return false;
    }
    if (remaining_ > 0) {
      return true;
    }
    char hex[12];
    int len = 0;
    for (;;) {
      int b = waitForByte(c_, timeout_ms_, true);
      if (b < 0) {
        done_ = true;
        return false;
      }
      if (b == '\r') {
        int nl = waitForByte(c_, timeout_ms_, true);
        if (nl != '\n') {
          done_ = true;
          return false;
        }
        break;
      }
      if (len < (int)sizeof(hex) - 1) {
        hex[len++] = static_cast<char>(b);
      }
    }
    hex[len] = '\0';
    remaining_ = static_cast<int>(strtol(hex, nullptr, 16));
    if (remaining_ <= 0) {
      // Final chunk (size 0) — drain the optional trailer + final \r\n.
      done_ = true;
      // Best-effort consume of trailing \r\n; if absent we're done anyway.
      waitForByte(c_, timeout_ms_, true);
      waitForByte(c_, timeout_ms_, true);
      return false;
    }
    return true;
  }

  WiFiClient &c_;
  uint32_t timeout_ms_;
  int remaining_;
  bool done_;
};

// Latched by the WiFi event handler when the AP rejects our credentials: the
// SSID was found and association started, but the 4-way WPA handshake failed or
// the AP sent an auth-related disconnect. File-scope because the ESP event
// callback is a plain C function pointer with no user context; there is a
// single Esp32Network instance in production, so this is safe.
volatile bool g_wifi_auth_failed = false;

void onWifiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  if (event != ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    return;
  }
  // Only the reason codes that mean "the AP actively rejected our credentials"
  // count as a terminal wrong-password. Deliberately NARROW:
  //   - AUTH_FAIL (202) / MIC_FAILURE (14): the AP refused the key outright.
  // Everything else is a transient we keep retrying. Notably excluded:
  //   - 4WAY_HANDSHAKE_TIMEOUT (15) / HANDSHAKE_TIMEOUT (204): the handshake
  //     frames were lost, which happens just as often on a congested/flaky AP
  //     with the CORRECT password. Field-observed: this fired mid-life (reason
  //     15) with an unchanged, correct PSK and wrongly latched the terminal
  //     "WLAN-PASSWORT FALSCH" screen. A lost handshake is not proof of a wrong
  //     key, so it must not be terminal — the normal retry loop recovers.
  //   - AUTH_EXPIRE (2): a routine re-auth timeout on an existing association
  //     (idle/roam/AP housekeeping), not a credential problem.
  const uint8_t reason = info.wifi_sta_disconnected.reason;
  switch (reason) {
  case WIFI_REASON_MIC_FAILURE:
  case WIFI_REASON_AUTH_FAIL:
    g_wifi_auth_failed = true;
    break;
  default:
    break;
  }
}

} // namespace

void Esp32Network::addAp(const char *ssid, const char *password) {
  wifi_.addAP(ssid, password);
  // Remember the SSID name (not the password) so the "KEIN EMPFANG" screen can
  // show which networks the device was looking for. Extra APs beyond the cap
  // still get added to WiFiMulti; they just aren't listed on-screen.
  if (ssid != nullptr && configured_.count < MAX_CONFIGURED_APS) {
    std::snprintf(configured_.ssid[configured_.count],
                  sizeof(configured_.ssid[0]), "%s", ssid);
    ++configured_.count;
  }
}

bool Esp32Network::connect(unsigned timeout_ms) {
  // Watch for the auth-failure disconnect reason during this attempt. Register
  // once; clear the latch each call so lastFailure() reflects only this run.
  //
  // MUST happen before WiFi.mode() starts the driver. WiFi.onEvent() is a bare
  // push_back onto the Arduino core's static `cbEventList` vector, with no lock
  // of any kind, while _arduino_event_task() concurrently iterates that same
  // vector and copies entries out of it by value (WiFiGeneric.cpp:1161). Once
  // the driver is up, that task is live and immediately posting events
  // (WIFI_READY/STA_START), so registering afterwards races the growth of an
  // empty vector against a reader: the reader sees the new size with the old
  // (freed/null) buffer, copies a garbage std::function out of it, and jumps
  // through its manager pointer — observed on a cold boot as
  //   Guru Meditation Error: Core 1 panic'ed (InstrFetchProhibited)
  //   PC: 0x0f03070f  ... WiFiEventCbList::WiFiEventCbList(WiFiEventCbList
  //   const&) <- WiFiGenericClass::_eventCallback <- _arduino_event_task
  // Registering while the driver is still down means the list is only ever
  // mutated with no event task running, and never again afterwards.
  if (!event_registered_) {
    WiFi.onEvent(onWifiEvent);
    event_registered_ = true;
  }

  // Pin the regulatory domain to Austria/ETSI (2.4 GHz channels 1-13). The
  // default world/US domain caps scanning at ch1-11, so an AP that has
  // auto-hopped to ch12/13 is invisible to the radio (observed in the field:
  // the home AP moved to ch12 and the board's scan went blank). MANUAL policy
  // keeps our range fixed regardless of any beacon country IE. Must run after
  // the driver is up (WiFi.mode) and before WiFiMulti's scan.
  WiFi.mode(WIFI_STA);
  const wifi_country_t at_country = {/*cc=*/"AT", /*schan=*/1, /*nchan=*/13,
                                     /*max_tx_power=*/0,
                                     /*policy=*/WIFI_COUNTRY_POLICY_MANUAL};
  esp_wifi_set_country(&at_country);

  g_wifi_auth_failed = false;
  last_failure_ = WifiFailure::None;

  if (wifi_.run(timeout_ms) == WL_CONNECTED) {
    return true;
  }
  // Wrong password: the AP was found and association started, but the WPA
  // handshake failed (reason 15 etc.). This is terminal — retrying with the
  // same credentials can never succeed — so classify it distinctly and let the
  // caller show the "WLAN-PASSWORT FALSCH" screen instead of the retry loop.
  if (g_wifi_auth_failed) {
    last_failure_ = WifiFailure::AuthFailed;
    Serial.println("[net] WPA handshake failed — wrong password (terminal)");
    return false;
  }
  last_failure_ = WifiFailure::NotFound;
  // No configured AP matched. WiFiMulti only logs the count ("N networks
  // found") — dump the actual SSIDs so a field diagnosis can tell whether the
  // home AP is simply absent, renamed, or on an out-of-range channel. This
  // re-scans (WiFiMulti already discarded its results — see scanVisible()); the
  // "KEIN EMPFANG" screen re-scans the same way.
  ScanResult scan = scanVisible();
  if (scan.count == 0) {
    Serial.println("[net] no matching AP; scan results unavailable");
    return false;
  }
  Serial.printf("[net] no matching AP; %d visible:\n", scan.count);
  for (int i = 0; i < scan.count; ++i) {
    Serial.printf("[net]   \"%s\" ch%d %ddBm\n", scan.aps[i].ssid,
                  scan.aps[i].channel, scan.aps[i].rssi_dbm);
  }
  // Case-only mismatch is the common footgun: 802.11 SSIDs are case-sensitive
  // and WiFiMulti compares them exactly, so a configured "A-NET2" never matches
  // a broadcast "a-net2" even though the AP is right there. Call it out.
  SsidCaseMismatch cm = findCaseMismatch(configured_, scan);
  if (cm.found) {
    Serial.printf("[net] CASE MISMATCH: looking for \"%s\" but found \"%s\" "
                  "(SSIDs are case-sensitive — fix casing in secrets.h)\n",
                  cm.configured, cm.visible);
  }
  return false;
}

bool Esp32Network::isConnected() { return WiFi.status() == WL_CONNECTED; }

ScanResult Esp32Network::scanVisible() {
  ScanResult r;
  // Run our own scan — do NOT rely on WiFiMulti's results. WiFiMulti::run()
  // calls WiFi.scanDelete() before it returns (even on the "no matching wifi
  // found" path), so scanComplete() reads back empty by the time we get here.
  // A fresh synchronous scan keeps its results valid until the next scan, so
  // the SSIDs below (and the "KEIN EMPFANG" list) actually have data.
  int found = WiFi.scanNetworks();
  for (int i = 0; i < found && r.count < SCAN_MAX_APS; ++i) {
    ScanEntry &e = r.aps[r.count];
    std::snprintf(e.ssid, sizeof(e.ssid), "%s", WiFi.SSID(i).c_str());
    e.rssi_dbm = static_cast<std::int8_t>(WiFi.RSSI(i));
    e.channel = static_cast<std::uint8_t>(WiFi.channel(i));
    ++r.count;
  }
  WiFi.scanDelete(); // free the scan RAM; we've copied what we need
  return r;
}

ConfiguredSsids Esp32Network::configuredSsids() { return configured_; }

WifiFailure Esp32Network::lastFailure() { return last_failure_; }

NetInfo Esp32Network::connectionInfo() {
  NetInfo ni;
  if (WiFi.status() != WL_CONNECTED) {
    return ni;
  }
  ni.valid = true;
  std::snprintf(ni.ssid, sizeof(ni.ssid), "%s", WiFi.SSID().c_str());
  std::snprintf(ni.ip, sizeof(ni.ip), "%s", WiFi.localIP().toString().c_str());
  ni.rssi_dbm = WiFi.RSSI();
  return ni;
}

HttpResult Esp32Network::httpGet(const std::string &url, std::string &out) {
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
    return {false, 0};
  }
  http.setTimeout(8000);
  int code = http.GET();
  if (code < 200 || code >= 300) {
    Serial.printf("[net] HTTP %d (non-2xx, aborting)\n", code);
    http.end();
    // Auth/4xx/5xx counts as "transport ok, semantic error" — pass status up
    // so callers can drive the auth-tripwire. Transport errors (negative
    // codes from HTTPClient) collapse to {false, 0}.
    return {code > 0, code > 0 ? code : 0};
  }
  int content_length = http.getSize();
  if (content_length > 0) {
    out.reserve(static_cast<size_t>(content_length));
  }
  StringAppender appender(out);
  int written = http.writeToStream(&appender);
  http.end();
  if (written < 0) {
    Serial.printf("[net] writeToStream failed: %d\n", written);
    return {false, 0};
  }
  Serial.printf("[net] HTTP %d, %u bytes\n", code,
                static_cast<unsigned>(out.size()));
  return {true, code};
}

HttpResult Esp32Network::httpPost(const std::string &url,
                                  const std::string &body,
                                  const std::string &content_type,
                                  std::string &out) {
  std::string().swap(out);
  Serial.printf("[net] POST %s (%u B) heap_free=%u largest=%u\n", url.c_str(),
                static_cast<unsigned>(body.size()),
                static_cast<unsigned>(ESP.getFreeHeap()),
                static_cast<unsigned>(
                    heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT)));
  // Reserve the buffer BEFORE the request, while the heap is still whole.
  // The reserve further down only fires when Content-Length is known, and
  // HAFAS sends its large bodies chunked (getSize() == -1) — so without this
  // the appends below climb the geometric ladder 16→32→64 KB, and the 64 KB
  // step needs a contiguous block *after* the TLS handshake has taken its
  // share. On a 9.5 h soak that failed ~every 3 min: operator new → no
  // exceptions on this target → std::terminate → abort() → chip reset, inside
  // StringAppender::write(). Sizing up front costs one transient allocation
  // out of ~195 KB free and removes the ladder entirely.
  out.reserve(POST_RESPONSE_RESERVE_BYTES);
  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure(); // HAFAS Let's Encrypt / DigiCert in system bundle
  if (!http.begin(client, url.c_str())) {
    Serial.println("[net] http.begin() failed");
    return {false, 0};
  }
  http.setTimeout(8000);
  http.addHeader("Content-Type", content_type.empty()
                                     ? String("application/json")
                                     : String(content_type.c_str()));
  int code =
      http.POST(reinterpret_cast<uint8_t *>(const_cast<char *>(body.data())),
                body.size());
  if (code < 200 || code >= 300) {
    Serial.printf("[net] HTTP %d (non-2xx, aborting)\n", code);
    // Still read the body — HAFAS error responses (err="AID"/"AUTH") are JSON
    // and the parser needs them. But only attempt if the transport handshake
    // produced a status; negative codes mean no body to read.
    if (code > 0) {
      int content_length = http.getSize();
      if (content_length > 0) {
        out.reserve(static_cast<size_t>(content_length));
      }
      StringAppender appender(out);
      http.writeToStream(&appender);
    }
    http.end();
    return {code > 0, code > 0 ? code : 0};
  }
  int content_length = http.getSize();
  if (content_length > 0) {
    out.reserve(static_cast<size_t>(content_length));
  }
  StringAppender appender(out);
  int written = http.writeToStream(&appender);
  http.end();
  if (written < 0) {
    Serial.printf("[net] writeToStream failed: %d\n", written);
    return {false, 0};
  }
  Serial.printf("[net] HTTP %d, %u bytes\n", code,
                static_cast<unsigned>(out.size()));
  return {true, code};
}

bool Esp32Network::httpGetStream(const std::string &url,
                                 StreamConsumer consumer) {
  Serial.printf("[net] STREAM GET %s heap_free=%u largest=%u\n", url.c_str(),
                static_cast<unsigned>(ESP.getFreeHeap()),
                static_cast<unsigned>(
                    heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT)));
  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure();
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
  int content_length = http.getSize();
  WiFiClient *raw = http.getStreamPtr();
  if (!raw) {
    Serial.println("[net] getStreamPtr() returned null");
    http.end();
    return false;
  }
  bool ok;
  if (content_length < 0) {
    // EFA uses Transfer-Encoding: chunked. getStreamPtr() exposes the raw
    // TCP bytes (HTTPClient only decodes chunked inside writeToStream/
    // getString), so strip the chunk framing ourselves before handing the
    // body to the consumer.
    ChunkedDecodingStream chunked(*raw, 8000);
    ok = consumer(chunked);
  } else {
    BlockingClientStream blocking(*raw, 8000);
    ok = consumer(blocking);
  }
  http.end();
  Serial.printf("[net] STREAM HTTP %d, consumer=%s heap_after=%u\n", code,
                ok ? "ok" : "fail", static_cast<unsigned>(ESP.getFreeHeap()));
  return ok;
}

HttpResult Esp32Network::httpPostStream(const std::string &url,
                                        const std::string &body,
                                        const std::string &content_type,
                                        StreamConsumer consumer) {
  Serial.printf("[net] STREAM POST %s (%u B) heap_free=%u largest=%u\n",
                url.c_str(), static_cast<unsigned>(body.size()),
                static_cast<unsigned>(ESP.getFreeHeap()),
                static_cast<unsigned>(
                    heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT)));
  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure(); // HAFAS Let's Encrypt / DigiCert in system bundle
  if (!http.begin(client, url.c_str())) {
    Serial.println("[net] http.begin() failed");
    return {false, 0};
  }
  http.setTimeout(8000);
  http.addHeader("Content-Type", content_type.empty()
                                     ? String("application/json")
                                     : String(content_type.c_str()));
  int code =
      http.POST(reinterpret_cast<uint8_t *>(const_cast<char *>(body.data())),
                body.size());
  if (code < 200 || code >= 300) {
    Serial.printf("[net] HTTP %d (non-2xx, aborting)\n", code);
    http.end();
    return {false, code > 0 ? code : 0};
  }
  int content_length = http.getSize();
  WiFiClient *raw = http.getStreamPtr();
  if (!raw) {
    Serial.println("[net] getStreamPtr() returned null");
    http.end();
    return {false, code};
  }
  bool ok;
  if (content_length < 0) {
    // HAFAS sends the big StationBoard bodies chunked. getStreamPtr() hands
    // out the raw TCP bytes (HTTPClient only decodes chunking inside
    // writeToStream/getString), so strip the framing before the consumer.
    ChunkedDecodingStream chunked(*raw, 8000);
    ok = consumer(chunked);
  } else {
    BlockingClientStream blocking(*raw, 8000);
    ok = consumer(blocking);
  }
  http.end();
  Serial.printf("[net] STREAM POST HTTP %d, consumer=%s heap_after=%u\n", code,
                ok ? "ok" : "fail", static_cast<unsigned>(ESP.getFreeHeap()));
  return {ok, code};
}

} // namespace bustaferl

#endif
