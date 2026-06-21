#include "api_fetcher.h"

#ifndef NATIVE_BUILD
#include <Arduino.h> // delay()
#endif

namespace bustaferl {

namespace {

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
    if (net.httpGet(url, body)) {
      out.ok = true;
      return out;
    }
    if (attempt < cfg.max_attempts) {
      sleepMsBetweenAttempts(cfg.backoff_ms_base * attempt);
    }
  }
  return out;
}

FetchOutcome fetchPostWithRetry(INetwork &net, const std::string &url,
                                const std::string &body,
                                const std::string &content_type,
                                std::string &out, const FetchConfig &cfg) {
  FetchOutcome o;
  for (int attempt = 1; attempt <= cfg.max_attempts; ++attempt) {
    o.attempts_taken = attempt;
    out.clear();
    if (net.httpPost(url, body, content_type, out)) {
      o.ok = true;
      return o;
    }
    if (attempt < cfg.max_attempts) {
      sleepMsBetweenAttempts(cfg.backoff_ms_base * attempt);
    }
  }
  return o;
}

} // namespace bustaferl
