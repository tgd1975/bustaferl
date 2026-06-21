#ifndef BUSTAFERL_SNAPSHOT_FETCHER_H
#define BUSTAFERL_SNAPSHOT_FETCHER_H

#include "../data/StreamSnapshot.h"
#include "../data/oebb_hafas_parse.h"
#include "../data/wienerlinien_parse.h"
#include "../hal/INetwork.h"

#include <string>

namespace bustaferl {

// Maximum stopIds per OGD monitor query. Smaller batches are empirically more
// stable — the all-five-at-once call occasionally dropped individual entries
// (observed: U1 Oberlaa missing on one call, present on the next). 2 stopIds
// per call → 3 batches for our 5 streams.
constexpr int STOPIDS_PER_QUERY = 2;

// Number of OGD (Wiener Linien) bus streams, fetched via the GET monitor
// batch. Equals the index of the first non-OGD stream (the S-Bahn), which is
// fetched separately via the ÖBB POST path.
constexpr int OGD_STREAM_COUNT = STREAM_SBAHN_HBF;

// Order in which the OGD stream slots are queried. STREAM_58A_ATZ is the lone
// trailing single-stop query; if data gaps appear there the problem follows
// query position, not the RBL.
extern const int FETCH_ORDER[OGD_STREAM_COUNT];

struct FetchSummary {
  int total_batches = 0;
  int failed_batches = 0;
  // True iff the ÖBB POST returned 2xx (regardless of the HAFAS err field).
  // Lets the cycle tell "no S-Bahn HTTP response" apart from "responded with
  // an auth error" for ÖBB auth-health tracking.
  bool oebb_http_ok = false;
};

// Compose the OGD monitor URL for one batch of stopIds. Caller supplies the
// endpoint base (production passes WL_API_BASE); a `&stopId=<id>` is appended
// for each entry in `stop_ids[0..count-1]`.
std::string apiUrlForBatch(const std::string &endpoint_base,
                           const int *stop_ids, int count);

// Fetch the OGD bus streams in batches of STOPIDS_PER_QUERY via `net.httpGet`
// (with the api_fetcher retry policy), then the S-Bahn stream via the ÖBB
// HAFAS POST (`net.httpPost`). `endpoint_base` is the OGD monitor prefix
// (config.h::WL_API_BASE); `mgate_url` the ÖBB mgate.exe endpoint
// (config.h::OEBB_MGATE_URL); `oebb_filter` the single S-Bahn filter. Both are
// passed in so host tests can route to fake endpoints.
//
// Sets `out.api_ok = true` iff at least one batch (OGD or ÖBB) returned a
// parsable response. `summary` is populated with the batch counters the
// snapshot summary log line consumes, plus `oebb_http_ok`. Returns the same
// flag as out.api_ok for the caller's convenience.
//
// Per-batch logging (HTTP failure, retry success, parse failure) stays inside
// this function via Serial.printf — that log is per-batch and not part of the
// snapshot summary block.
bool fetchSnapshot(INetwork &net, const std::string &endpoint_base,
                   const std::string &mgate_url,
                   const StreamFilter (&filters)[STREAM_COUNT],
                   const OebbStreamFilter &oebb_filter, StreamSnapshot &out,
                   FetchSummary &summary);

} // namespace bustaferl

#endif
