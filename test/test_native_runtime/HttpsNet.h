#ifndef BUSTAFERL_NATIVE_RUNTIME_HTTPSNET_H
#define BUSTAFERL_NATIVE_RUNTIME_HTTPSNET_H

#include "../../src/hal/INetwork.h"

#include <string>

namespace bustaferl::native_runtime {

// libcurl-backed INetwork for the host runtime. Goes against the real Wiener
// Linien endpoints by default; consumers can flip to a mock by setting
// `MOCK_API_BASE` in the URL — HttpsNet itself does not rewrite, the driver
// chooses the base URL. Connect/isConnected are no-ops because the host has
// real network already; the cycle's WiFi gate just returns true.
//
// Timeout mirrors Esp32Network (8 s). Cert verification uses libcurl's
// system default chain; opt out with `setInsecure(true)` for the mock path
// when the runner serves over plain HTTP / self-signed.
class HttpsNet : public INetwork {
public:
  HttpsNet();
  ~HttpsNet() override;

  HttpsNet(const HttpsNet &) = delete;
  HttpsNet &operator=(const HttpsNet &) = delete;

  bool connect(unsigned /*timeout_ms*/) override { return true; }
  bool isConnected() override { return true; }
  bool httpGet(const std::string &url, std::string &out) override;
  bool httpPost(const std::string &url, const std::string &body,
                const std::string &content_type, std::string &out) override;

  // Disable TLS verification (needed for the local mock runner). Off by
  // default — production calls go through the system trust store.
  void setInsecure(bool insecure) { insecure_ = insecure; }

private:
  bool insecure_ = false;
};

} // namespace bustaferl::native_runtime

#endif
