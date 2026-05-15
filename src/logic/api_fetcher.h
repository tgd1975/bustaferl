#ifndef BUSTAFERL_API_FETCHER_H
#define BUSTAFERL_API_FETCHER_H

#include <string>

#include "../hal/INetwork.h"

namespace bustaferl {

struct FetchConfig {
  int max_attempts = 3;
  // Linear backoff: sleep `backoff_ms_base * attempt` between attempts.
  // 500ms base → 500, 1000ms waits before the 2nd and 3rd attempts.
  int backoff_ms_base = 500;
};

struct FetchOutcome {
  bool ok = false;
  int attempts_taken = 0; // populated whether or not ok is true
};

// Calls `net.httpGet(url, body)` up to `cfg.max_attempts` times. Returns the
// outcome including how many attempts were taken so callers (and tests) can
// surface "succeeded on the 2nd try" in logs. Linear backoff between
// attempts; on host build the backoff is a no-op so tests stay fast.
FetchOutcome fetchWithRetry(INetwork &net, const std::string &url,
                            std::string &body, const FetchConfig &cfg);

} // namespace bustaferl

#endif
