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

// Order in which stream slots are queried. Reversed from display/enum order so
// STREAM_U1_OBERLAA moves into the first paired batch instead of being the
// lone 5th query. Diagnostic: if data gaps now appear on STREAM_58A_ATZ (the
// new singleton), the problem follows query position; if they still appear on
// U1-Oberlaa, the problem is RBL-specific.
extern const int FETCH_ORDER[STREAM_COUNT];

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
