#ifndef BUSTAFERL_SNAPSHOT_FETCHER_H
#define BUSTAFERL_SNAPSHOT_FETCHER_H

#include "../data/StreamSnapshot.h"
#include "../data/wienerlinien_parse.h"
#include "../hal/INetwork.h"

#include <string>

namespace bustaferl {

// Maximum stopIds per OGD monitor query. Smaller batches are empirically more
// stable — the all-five-at-once call occasionally dropped individual entries
// (observed: U1 Oberlaa missing on one call, present on the next). 2 stopIds
// per call → 3 batches for our 5 streams.
constexpr int STOPIDS_PER_QUERY = 2;

// Order in which OGD stream slots are queried. v2: only the three bus
// streams. STREAM_SBAHN_HBF is handled out-of-band by Schritt 5's
// `fetchOebbStream` (HAFAS mgate), so it is intentionally absent here —
// re-adding it would issue an OGD call with stopId=0.
constexpr int OGD_FETCH_COUNT = 3;
extern const int FETCH_ORDER[OGD_FETCH_COUNT];

struct FetchSummary {
  int total_batches = 0;
  int failed_batches = 0;
};

// Compose the OGD monitor URL for one batch of stopIds. Caller supplies the
// endpoint base (production passes WL_API_BASE); a `&stopId=<id>` is appended
// for each entry in `stop_ids[0..count-1]`.
std::string apiUrlForBatch(const std::string &endpoint_base,
                           const int *stop_ids, int count);

// Iterate the 5 streams in batches of STOPIDS_PER_QUERY, fetch each batch via
// `net.httpGet` (with the api_fetcher retry policy), parse the response, and
// merge the per-stream results into `out`. `endpoint_base` is the URL prefix
// production code reads from config.h::WL_API_BASE — passed in so host tests
// can route to a fake endpoint.
//
// Sets `out.api_ok = true` iff at least one batch returned a parsable
// response. `summary` is populated with the batch counters that the snapshot
// summary log line consumes. Returns the same flag as out.api_ok for the
// caller's convenience.
//
// Per-batch logging (HTTP failure, retry success, parse failure) stays inside
// this function via Serial.printf — that log is per-batch and not part of the
// snapshot summary block extracted in Schritt 3.
bool fetchSnapshot(INetwork &net, const std::string &endpoint_base,
                   const StreamFilter (&filters)[STREAM_COUNT],
                   StreamSnapshot &out, FetchSummary &summary);

} // namespace bustaferl

#endif
