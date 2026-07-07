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

// Per-call outcome.
//
//   - `ok == true`  → at least one attempt produced a 2xx response. `body`
//     holds that response.
//   - `ok == false` → no 2xx attempt; either transport-fail (`http_status`
//     == 0) or the server returned 4xx/5xx. `body` may carry the last
//     error body for log inspection on 5xx termination.
//
// The retry policy (see fetchWithRetry / fetchPostWithRetry) terminates
// after the first 401/403/non-retryable-4xx — callers see the auth status
// without waiting for max_attempts.
struct FetchOutcome {
  bool ok = false;
  int attempts_taken = 0; // populated whether or not ok is true
  int http_status = 0;    // last observed HTTP status (0 on transport error)
};

// Calls `net.httpGet(url, body)` up to `cfg.max_attempts` times. Retry
// policy (matches the table in v2-sbahn-migration-plan §5.6):
//
//   http_status == 0                  → retry (transport-class failure)
//   2xx                               → return immediately (success)
//   401 / 403                         → return immediately (auth tripwire)
//   408 / 429 / 5xx                   → retry (server-class transient)
//   other 4xx                         → return immediately (client bug)
//
// After max_attempts of transport failures the result is `{false, 0}` with
// an empty body. After max_attempts of 5xx the result is `{true,
// last_status}` and `body` holds the last error body (for log inspection).
FetchOutcome fetchWithRetry(INetwork &net, const std::string &url,
                            std::string &body, const FetchConfig &cfg);

// Same retry policy, POST semantics. `content_type` typically
// "application/json; charset=UTF-8" for HAFAS.
FetchOutcome fetchPostWithRetry(INetwork &net, const std::string &url,
                                const std::string &request_body,
                                const std::string &content_type,
                                std::string &response_body,
                                const FetchConfig &cfg);

} // namespace bustaferl

#endif
