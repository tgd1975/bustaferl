#include "logic/snapshot_fetcher.h"

#include "logic/api_fetcher.h"

#include <chrono>
#include <cstdio>

#ifndef NATIVE_BUILD
#include <Arduino.h>
#define SNAP_LOG(...) Serial.printf(__VA_ARGS__)
#define SNAP_FREE_HEAP() static_cast<uint32_t>(ESP.getFreeHeap())
#else
#define SNAP_LOG(...) ((void)0)
#define SNAP_FREE_HEAP() static_cast<uint32_t>(0)
#endif

namespace bustaferl {

// v2: only the three OGD bus streams are fetched via the monitor endpoint.
// STREAM_SBAHN_HBF is filled by fetchOebbStream below (HAFAS mgate).
const int FETCH_ORDER[OGD_FETCH_COUNT] = {
    STREAM_58B_ATZ,
    STREAM_58A_HIETZING,
    STREAM_58A_ATZ,
};

namespace {

// stopId fragment: "&stopId=" + up to ~10 digit RBL + null.
constexpr int STOPID_FRAG_BUF = 24;
// Batch label: comma-separated RBLs, e.g. "1234567,7654321".
constexpr int BATCH_LABEL_BUF = 40;

constexpr int HTTP_401 = 401;
constexpr int HTTP_403 = 403;

void formatBatchLabel(char *out, std::size_t out_size, const int *stop_ids,
                      int count) {
  int pos = 0;
  for (int j = 0; j < count; ++j) {
    pos += std::snprintf(out + pos, out_size - pos, j == 0 ? "%d" : ",%d",
                         stop_ids[j]);
  }
}

bool isAuthCode(int status) { return status == HTTP_401 || status == HTTP_403; }

// Single HAFAS mgate.exe POST → parseOebbStationBoard. Updates summary like
// the OGD batch loop (counts as one batch). The result struct's flags are
// copied into the StreamData so snapshot_logger / filter_health see the
// same values they get from the OGD path.
void fetchOebbStream(INetwork &net, const FetchInputs &inputs, time_t now,
                     StreamSnapshot &out, FetchSummary &summary,
                     PersistedMeta &meta) {
  std::string body = buildOebbRequest(inputs.oebb_filter);
  std::string resp;
  FetchConfig fc;
  FetchOutcome fo = fetchPostWithRetry(
      net, inputs.mgate_url, body, "application/json; charset=UTF-8", resp, fc);
  ++summary.total_batches;

  if (!fo.ok) {
    SNAP_LOG("[api] oebb httpPost failed after %d attempts (status=%d)\n",
             fo.attempts_taken, fo.http_status);
    ++summary.failed_batches;
    return;
  }
  if (fo.attempts_taken > 1) {
    SNAP_LOG("[api] oebb succeeded on attempt %d/%d\n", fo.attempts_taken,
             fc.max_attempts);
  }

  OebbParseResult pr;
  StreamData parsed;
  if (!parseOebbStationBoard(resp, now, parsed, pr)) {
    SNAP_LOG("[api] oebb parse failed\n");
    ++summary.failed_batches;
    return;
  }

  out.stream[STREAM_SBAHN_HBF] = parsed;
  out.stream[STREAM_SBAHN_HBF].endpoint_responded = pr.endpoint_responded;
  out.stream[STREAM_SBAHN_HBF].filter_matched = pr.filter_matched;

  if (pr.auth_error_seen) {
    SNAP_LOG("[api] oebb auth_error_seen=1\n");
    meta.auth_error_seen = true;
  } else if (pr.endpoint_responded) {
    // HAFAS antwortet wieder normal — Auth-Drift ist weg. Den OGD-Streak
    // räumt die OGD-Schleife (siehe runOgdBatchLoop).
    meta.auth_error_seen = false;
  }
}

void runOgdBatchLoop(INetwork &net, const std::string &endpoint_base,
                     const StreamFilter (&filters)[STREAM_COUNT],
                     StreamSnapshot &out, FetchSummary &summary,
                     PersistedMeta &meta) {
  for (int start = 0; start < OGD_FETCH_COUNT; start += STOPIDS_PER_QUERY) {
    int batch_size = OGD_FETCH_COUNT - start;
    if (batch_size > STOPIDS_PER_QUERY)
      batch_size = STOPIDS_PER_QUERY;

    int stop_ids[STOPIDS_PER_QUERY] = {0};
    for (int j = 0; j < batch_size; ++j) {
      stop_ids[j] = filters[FETCH_ORDER[start + j]].rbl;
    }

    // cppcheck-suppress variableScope // consumed only by SNAP_LOG below,
    // which is a no-op on NATIVE_BUILD — keep adjacent to its producer.
    char batch_label[BATCH_LABEL_BUF] = "";
    formatBatchLabel(batch_label, sizeof(batch_label), stop_ids, batch_size);

    std::string body;
    FetchConfig fc;
    FetchOutcome fo = fetchWithRetry(
        net, apiUrlForBatch(endpoint_base, stop_ids, batch_size), body, fc);
    ++summary.total_batches;

    if (!fo.ok) {
      SNAP_LOG(
          "[api] batch [%s] httpGet failed after %d attempts (status=%d)\n",
          batch_label, fo.attempts_taken, fo.http_status);
      ++summary.failed_batches;
      // Auth-Tripwire: 401/403 zählt — drei am Stück flippen
      // auth_error_seen. Transport-Fails (status=0) lassen den Streak in
      // Ruhe.
      if (isAuthCode(fo.http_status)) {
        if (meta.ogd_auth_streak < OGD_AUTH_STREAK_TRIPWIRE) {
          ++meta.ogd_auth_streak;
        }
        if (meta.ogd_auth_streak >= OGD_AUTH_STREAK_TRIPWIRE) {
          meta.auth_error_seen = true;
        }
      }
      continue;
    }
    if (fo.attempts_taken > 1) {
      SNAP_LOG("[api] batch [%s] succeeded on attempt %d/%d\n", batch_label,
               fo.attempts_taken, fc.max_attempts);
    }
    meta.ogd_auth_streak = 0;

    StreamSnapshot partial;
    if (!parseMonitorResponse(body, filters, partial)) {
      SNAP_LOG("[api] batch [%s] parse failed\n", batch_label);
      ++summary.failed_batches;
      continue;
    }

    // Copy out only the streams we asked for in this batch — other indices
    // in `partial` are default-empty by construction.
    for (int j = 0; j < batch_size; ++j) {
      int idx = FETCH_ORDER[start + j];
      out.stream[idx] = partial.stream[idx];
    }
  }
}

} // namespace

std::string apiUrlForBatch(const std::string &endpoint_base,
                           const int *stop_ids, int count) {
  std::string url = endpoint_base;
  char buf[STOPID_FRAG_BUF];
  for (int i = 0; i < count; ++i) {
    std::snprintf(buf, sizeof(buf), "&stopId=%d", stop_ids[i]);
    url += buf;
  }
  return url;
}

bool fetchSnapshot(INetwork &net, const FetchInputs &inputs, time_t now,
                   StreamSnapshot &out, FetchSummary &summary,
                   PersistedMeta &meta) {
  using clock = std::chrono::steady_clock;
  using std::chrono::duration_cast;
  using std::chrono::milliseconds;

  out = StreamSnapshot{};
  summary = FetchSummary{};

  // OGD first — its calls are cheap and its body is small. A HAFAS failure
  // shouldn't suppress fresh bus times.
  const auto t0 = clock::now();
  runOgdBatchLoop(net, inputs.endpoint_base, inputs.filters, out, summary,
                  meta);
  const auto t1 = clock::now();
  summary.ogd_ms =
      static_cast<uint32_t>(duration_cast<milliseconds>(t1 - t0).count());

  // Then the single HAFAS call. mgate_url empty → host test that does not
  // exercise the OEBB path; skip.
  if (!inputs.mgate_url.empty()) {
    summary.free_heap_before_oebb = SNAP_FREE_HEAP();
    fetchOebbStream(net, inputs, now, out, summary, meta);
    summary.free_heap_after_oebb = SNAP_FREE_HEAP();
    const auto t2 = clock::now();
    summary.oebb_ms =
        static_cast<uint32_t>(duration_cast<milliseconds>(t2 - t1).count());
  }

  // api_ok if at least one batch returned valid JSON.
  out.api_ok = (summary.failed_batches < summary.total_batches);
  return out.api_ok;
}

} // namespace bustaferl
