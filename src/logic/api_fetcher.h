#ifndef BUSTAFERL_API_FETCHER_H
#define BUSTAFERL_API_FETCHER_H

#include "../hal/INetwork.h"

#include <string>

namespace bustaferl {

constexpr int DEFAULT_FETCH_BACKOFF_MS_BASE = 500;
constexpr int DEFAULT_FETCH_MAX_ATTEMPTS = 5;

struct FetchConfig {
  // 5 attempts per batch: the OGD monitor drops individual calls often enough
  // that 3 attempts still left too many cycles without live data. Worst case
  // per batch is ~5 s of backoff — acceptable, the rescue window (see
  // logic/rescue_policy.h) covers what still slips through.
  int max_attempts = DEFAULT_FETCH_MAX_ATTEMPTS;
  // Linear backoff: sleep `backoff_ms_base * attempt` between attempts.
  // 500ms base → 500, 1000, 1500, 2000ms waits before attempts 2..5.
  int backoff_ms_base = DEFAULT_FETCH_BACKOFF_MS_BASE;
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

// POST variant: calls `net.httpPost(url, body, content_type, out)` up to
// `cfg.max_attempts` times with the same linear backoff. Used by the ÖBB
// HAFAS S-Bahn fetch.
FetchOutcome fetchPostWithRetry(INetwork &net, const std::string &url,
                                const std::string &body,
                                const std::string &content_type,
                                std::string &out, const FetchConfig &cfg);

} // namespace bustaferl

#endif
