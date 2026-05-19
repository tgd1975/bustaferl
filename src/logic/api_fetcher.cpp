#include "api_fetcher.h"

#include <cstdint>

#ifndef NATIVE_BUILD
#include <Arduino.h> // delay()
#endif

namespace bustaferl {

namespace {

constexpr int HTTP_2XX_MIN = 200;
constexpr int HTTP_3XX_MIN = 300;
constexpr int HTTP_4XX_MIN = 400;
constexpr int HTTP_5XX_MAX = 600;
constexpr int HTTP_408 = 408;
constexpr int HTTP_429 = 429;
constexpr int HTTP_5XX_MIN = 500;
constexpr int HTTP_401 = 401;
constexpr int HTTP_403 = 403;

void sleepMsBetweenAttempts(int ms) {
#ifndef NATIVE_BUILD
  if (ms > 0)
    delay(ms);
#else
  (void)ms;
#endif
}

// Klassifiziert eine HTTP-Antwort gegen die Retry-Tabelle aus §5.6.
// Rückgabewerte: 0 = sofort zurück (Erfolg / nicht-retryable), 1 = retry.
enum class Disposition : std::uint8_t { ReturnNow, Retry };

Disposition classify(HttpResult r) {
  if (!r.ok)
    return Disposition::Retry; // transport fail
  int s = r.http_status;
  if (s >= HTTP_2XX_MIN && s < HTTP_3XX_MIN)
    return Disposition::ReturnNow; // 2xx
  if (s == HTTP_401 || s == HTTP_403)
    return Disposition::ReturnNow; // auth tripwire
  if (s == HTTP_408 || s == HTTP_429 || (s >= HTTP_5XX_MIN && s < HTTP_5XX_MAX))
    return Disposition::Retry; // server-side transient
  if (s >= HTTP_4XX_MIN && s < HTTP_5XX_MIN)
    return Disposition::ReturnNow; // other 4xx → caller bug
  return Disposition::ReturnNow;
}

bool isSuccess(int status) {
  return status >= HTTP_2XX_MIN && status < HTTP_3XX_MIN;
}

// Gemeinsamer Retry-Loop für GET und POST. `do_call` führt eine einzelne
// Request-Iteration aus und schreibt den Antwort-Body in `body`.
//
// Vertragsdetail (anders als die §5.6-Tabelle nahelegt): `FetchOutcome.ok`
// bleibt im Bestandsverhalten "≥1 erfolgreicher 2xx-Versuch". Auth-Codes
// (401/403) und übrige 4xx terminieren zwar nach dem ersten Treffer (kein
// sinnloses Retry), führen aber zu `ok=false` plus dem konkreten Status —
// existierende Caller (snapshot_fetcher, schedule_fetcher, schedule_refresh)
// dürfen `!fo.ok` weiter als "keine Daten" lesen. Auth-Tripwires hängen
// stattdessen am `http_status` (siehe v2-Plan §5.3 ogd_auth_streak-Pfad).
template <typename Call>
FetchOutcome runRetryLoop(std::string &body, const FetchConfig &cfg,
                          Call do_call) {
  FetchOutcome out;
  for (int attempt = 1; attempt <= cfg.max_attempts; ++attempt) {
    out.attempts_taken = attempt;
    body.clear();
    HttpResult r = do_call(body);
    out.http_status = r.http_status;
    Disposition d = classify(r);
    if (d == Disposition::ReturnNow) {
      out.ok = r.ok && isSuccess(r.http_status);
      return out;
    }
    if (attempt < cfg.max_attempts) {
      sleepMsBetweenAttempts(cfg.backoff_ms_base * attempt);
    } else {
      // Retry-Pfad erschöpft. ok bleibt false; bei 5xx halten wir Status
      // + last_body für Log-Inspektion (Body steht in `body`).
      out.ok = false;
    }
  }
  return out;
}

} // namespace

FetchOutcome fetchWithRetry(INetwork &net, const std::string &url,
                            std::string &body, const FetchConfig &cfg) {
  return runRetryLoop(body, cfg,
                      [&](std::string &b) { return net.httpGet(url, b); });
}

FetchOutcome fetchPostWithRetry(INetwork &net, const std::string &url,
                                const std::string &request_body,
                                const std::string &content_type,
                                std::string &response_body,
                                const FetchConfig &cfg) {
  return runRetryLoop(response_body, cfg, [&](std::string &b) {
    return net.httpPost(url, request_body, content_type, b);
  });
}

} // namespace bustaferl
