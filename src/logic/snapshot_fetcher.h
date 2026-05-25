#ifndef BUSTAFERL_SNAPSHOT_FETCHER_H
#define BUSTAFERL_SNAPSHOT_FETCHER_H

#include "../data/StreamSnapshot.h"
#include "../data/oebb_hafas_parse.h"
#include "../data/wienerlinien_parse.h"
#include "../hal/INetwork.h"
#include "../hal/IPersistentStore.h"

#include <cstdint>
#include <string>

namespace bustaferl {

// Maximum stopIds per OGD monitor query. Smaller batches are empirically more
// stable — the all-at-once call (v1, five streams) occasionally dropped
// individual entries from the response. 2 stopIds per call → 2 batches for
// v2's three bus streams (1 of 2 + 1 of 1).
constexpr int STOPIDS_PER_QUERY = 2;

// Order in which OGD stream slots are queried. v2: only the three bus
// streams. STREAM_SBAHN_HBF is handled out-of-band by Schritt 5's
// `fetchOebbStream` (HAFAS mgate), so it is intentionally absent here —
// re-adding it would issue an OGD call with stopId=0.
constexpr int OGD_FETCH_COUNT = 3;
extern const int FETCH_ORDER[OGD_FETCH_COUNT];

// Three consecutive OGD 401/403 responses promote auth_error_seen=true.
// Single 401s on the OGD path do happen during DNS / TLS edge cases, so we
// debounce. HAFAS auth on the other hand is driven purely by parser err.
constexpr uint8_t OGD_AUTH_STREAK_TRIPWIRE = 3;

struct FetchSummary {
  int total_batches = 0;
  int failed_batches = 0;
  // Schritt 9.4 sleep-budget instrumentation. Wall-clock per leg (ms) so
  // cycle_runner / device test can flag a wake exceeding ~10 s and decide
  // whether V13 mitigation is needed (session resumption, parallelisation,
  // every-other-cycle OEBB). 0 on legs that did not run.
  uint32_t ogd_ms = 0;
  uint32_t oebb_ms = 0;
  // Schritt 9.3 heap-budget instrumentation. Captured immediately before and
  // after the HAFAS POST so we can see how close the run got to the EFA heap
  // guard (~90 KB free). 0 on NATIVE_BUILD where ESP heap APIs are absent.
  uint32_t free_heap_before_oebb = 0;
  uint32_t free_heap_after_oebb = 0;
};

// Cycle-invariant inputs for fetchSnapshot(). Bundling them keeps the
// public signature within the readability-function-size param threshold and
// matches how the caller (cycle_runner) builds them once per boot and reuses
// them across warm cycles.
struct FetchInputs {
  const std::string &endpoint_base; // OGD API base URL
  const std::string &mgate_url;     // HAFAS mgate.exe URL
  const StreamFilter (&filters)[STREAM_COUNT];
  const OebbStreamFilter &oebb_filter;
};

// Compose the OGD monitor URL for one batch of stopIds. Caller supplies the
// endpoint base (production passes WL_API_BASE); a `&stopId=<id>` is appended
// for each entry in `stop_ids[0..count-1]`.
std::string apiUrlForBatch(const std::string &endpoint_base,
                           const int *stop_ids, int count);

// v2 fetchSnapshot: runs the OGD batch loop for the three bus streams and
// then a single HAFAS mgate.exe POST for the S-Bahn stream.
//
// `meta` is read-write because the fetcher maintains the auth-tripwire state:
//
//   - On the OGD path, three consecutive 401/403 responses flip
//     `meta.ogd_auth_streak` past OGD_AUTH_STREAK_TRIPWIRE and set
//     `meta.auth_error_seen = true`. A successful 2xx resets the streak.
//   - On the HAFAS path, the parser's `auth_error_seen` flag (err=="AID"/
//     "AUTH") flips `meta.auth_error_seen = true` immediately. A successful
//     err=="OK" parse clears the flag.
//
// `inputs.oebb_filter` is the single S-Bahn filter built by buildOebbFilter().
// Returns true iff at least one batch (OGD *or* HAFAS) produced valid data.
bool fetchSnapshot(INetwork &net, const FetchInputs &inputs,
                   StreamSnapshot &out, FetchSummary &summary,
                   PersistedMeta &meta);

} // namespace bustaferl

#endif
