#include "api_fetcher.h"

#ifndef NATIVE_BUILD
#include <Arduino.h> // delay()
#endif

namespace bustaferl {

namespace {

constexpr int HTTP_2XX_MIN = 200;
constexpr int HTTP_3XX_MIN = 300;

void sleepMsBetweenAttempts(int ms) {
#ifndef NATIVE_BUILD
  if (ms > 0)
    delay(ms);
#else
  (void)ms;
#endif
}

} // namespace

FetchOutcome fetchWithRetry(INetwork &net, const std::string &url,
                            std::string &body, const FetchConfig &cfg) {
  FetchOutcome out;
  for (int attempt = 1; attempt <= cfg.max_attempts; ++attempt) {
    out.attempts_taken = attempt;
    body.clear();
    HttpResult r = net.httpGet(url, body);
    out.http_status = r.http_status;
    if (r.ok && r.http_status >= HTTP_2XX_MIN &&
        r.http_status < HTTP_3XX_MIN) {
      out.ok = true;
      return out;
    }
    // Auth-class statuses (401/403) are not retried in Schritt 5.6's policy,
    // but Session B keeps the conservative behavior (retry-everything) until
    // the OEBB pipeline lands. The OGD path has been retrying transport-fails
    // for months without issue, so no churn here.
    if (attempt < cfg.max_attempts) {
      sleepMsBetweenAttempts(cfg.backoff_ms_base * attempt);
    }
  }
  return out;
}

} // namespace bustaferl
