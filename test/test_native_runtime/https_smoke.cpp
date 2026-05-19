// Standalone live-call smoke for HttpsNet. Not part of any PIO test env —
// built ad-hoc with the make target (see Makefile: native-runtime-https-smoke).
// Hits the production Wiener Linien endpoints three times and prints body
// sizes. Intentionally minimal: zero asserts, zero leak-checks; the validation
// gate from §9.2 ("3 manual smoke calls, Body-Size > 1 kB, JSON-monitors-key
// present") is read off the printed output.

#include "HttpsNet.cpp" // NOLINT(bugprone-suspicious-include)

#include <cstdio>
#include <string>

namespace {

constexpr const char *kRealtimeUrl =
    "https://www.wienerlinien.at/ogd_realtime/monitor?rbl=1234"; // any rbl
constexpr const char *kEfaUrl =
    "https://www.wienerlinien.at/ogd_realtime/monitor?rbl=4321";
constexpr const char *kPing = "https://www.wienerlinien.at/";

void hit(bustaferl::native_runtime::HttpsNet &net, const char *url) {
  std::string body;
  const auto r = net.httpGet(url, body);
  const bool ok = r.ok && r.http_status >= 200 && r.http_status < 300;
  const bool has_monitors =
      body.find("\"monitors\"") != std::string::npos ||
      body.find("<html") != std::string::npos; // ping URL is HTML
  std::printf("[https_smoke] %s ok=%d status=%d bytes=%zu monitors_or_html=%d\n",
              url, ok ? 1 : 0, r.http_status, body.size(),
              has_monitors ? 1 : 0);
}

} // namespace

int main() {
  bustaferl::native_runtime::HttpsNet net;
  hit(net, kRealtimeUrl);
  hit(net, kEfaUrl);
  hit(net, kPing);
  return 0;
}
