#include "Esp32Network.h"

#ifndef NATIVE_BUILD

#include <HTTPClient.h>
#include <Stream.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <cstdio>

namespace bustaferl {

namespace {

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

} // namespace

void Esp32Network::addAp(const char *ssid, const char *password) {
  wifi_.addAP(ssid, password);
}

bool Esp32Network::connect(unsigned timeout_ms) {
  return wifi_.run(timeout_ms) == WL_CONNECTED;
}

bool Esp32Network::isConnected() { return WiFi.status() == WL_CONNECTED; }

bool Esp32Network::connectionInfo(NetInfo &out) {
  if (WiFi.status() != WL_CONNECTED)
    return false;
  std::snprintf(out.ssid, sizeof(out.ssid), "%s", WiFi.SSID().c_str());
  std::snprintf(out.ip, sizeof(out.ip), "%s",
                WiFi.localIP().toString().c_str());
  out.rssi_dbm = WiFi.RSSI();
  return true;
}

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
  int content_length = http.getSize();
  if (content_length > 0) {
    out.reserve(static_cast<size_t>(content_length));
  }
  StringAppender appender(out);
  int written = http.writeToStream(&appender);
  http.end();
  if (written < 0) {
    Serial.printf("[net] writeToStream failed: %d\n", written);
    return false;
  }
  Serial.printf("[net] HTTP %d, %u bytes\n", code,
                static_cast<unsigned>(out.size()));
  return true;
}

bool Esp32Network::httpPost(const std::string &url, const std::string &body,
                            const std::string &content_type, std::string &out) {
  std::string().swap(out);
  Serial.printf("[net] POST %s heap_free=%u largest=%u\n", url.c_str(),
                static_cast<unsigned>(ESP.getFreeHeap()),
                static_cast<unsigned>(
                    heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT)));
  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure(); // ÖBB Let's Encrypt/DigiCert; CA bundle not shipped
  if (!http.begin(client, url.c_str())) {
    Serial.println("[net] http.begin() failed");
    return false;
  }
  http.setTimeout(8000);
  http.addHeader("Content-Type", content_type.c_str());
  int code =
      http.POST(reinterpret_cast<uint8_t *>(const_cast<char *>(body.data())),
                body.size());
  if (code < 200 || code >= 300) {
    Serial.printf("[net] HTTP %d (non-2xx, aborting)\n", code);
    http.end();
    return false;
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
    return false;
  }
  Serial.printf("[net] HTTP %d, %u bytes\n", code,
                static_cast<unsigned>(out.size()));
  return true;
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

} // namespace bustaferl

#endif
