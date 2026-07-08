#include "logic/snapshot_fetcher.h"

#include "logic/api_fetcher.h"

#include <cstdio>

#ifndef NATIVE_BUILD
#include <Arduino.h>
#define SNAP_LOG(...) Serial.printf(__VA_ARGS__)
#else
#define SNAP_LOG(...) ((void)0)
#endif

namespace bustaferl {

const int FETCH_ORDER[OGD_STREAM_COUNT] = {
    STREAM_58B_ATZ,
    STREAM_58A_HIETZING,
    STREAM_58A_ATZ,
};

namespace {

// stopId fragment: "&stopId=" + up to ~10 digit RBL + null.
constexpr int STOPID_FRAG_BUF = 24;
// Batch label: comma-separated RBLs, e.g. "1234567,7654321".
constexpr int BATCH_LABEL_BUF = 40;

// Comma-joined stopIds for the per-batch SNAP_LOG diagnostics. Hot enough that
// keeping it out of the request-pump loop body matters for readability.
void formatBatchLabel(char *out, std::size_t out_size, const int *stop_ids,
                      int count) {
  int pos = 0;
  for (int j = 0; j < count; ++j) {
    pos += std::snprintf(out + pos, out_size - pos, j == 0 ? "%d" : ",%d",
                         stop_ids[j]);
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

// One ÖBB HAFAS StationBoard POST → parse into out.stream[STREAM_SBAHN_HBF].
// Counts as one batch in `summary`; sets summary.oebb_http_ok on a 2xx so the
// cycle can run auth-health off the parsed endpoint_responded flag.
static bool fetchOebbStream(INetwork &net, const std::string &mgate_url,
                            const OebbStreamFilter &filter, StreamSnapshot &out,
                            FetchSummary &summary) {
  std::string body = buildOebbRequest(filter);
  std::string resp;
  FetchConfig fc;
  FetchOutcome fo =
      fetchPostWithRetry(net, mgate_url, body, "application/json", resp, fc);
  ++summary.total_batches;
  if (!fo.ok) {
    SNAP_LOG("[api] oebb httpPost failed after %d attempts\n",
             fo.attempts_taken);
    ++summary.failed_batches;
    return false;
  }
  summary.oebb_http_ok = true;
  if (fo.attempts_taken > 1) {
    ++summary.retried_batches;
    SNAP_LOG("[api] oebb succeeded on attempt %d/%d\n", fo.attempts_taken,
             fc.max_attempts);
  }
  if (!parseOebbStationBoard(resp, out.stream[STREAM_SBAHN_HBF])) {
    SNAP_LOG("[api] oebb parse failed\n");
    ++summary.failed_batches;
    return false;
  }
  return true;
}

// 7 parameters: two endpoints + the two filter shapes (OGD table, single ÖBB
// struct) + two outputs. They have no shared invariant — wrapping them into a
// struct would only add a pass-through layer for every caller and test.
// NOLINTNEXTLINE(readability-function-size)
bool fetchSnapshot(INetwork &net, const std::string &endpoint_base,
                   const std::string &mgate_url,
                   const StreamFilter (&filters)[STREAM_COUNT],
                   const OebbStreamFilter &oebb_filter, StreamSnapshot &out,
                   FetchSummary &summary) {
  out = StreamSnapshot{};
  summary = FetchSummary{};

  for (int start = 0; start < OGD_STREAM_COUNT; start += STOPIDS_PER_QUERY) {
    int batch_size = OGD_STREAM_COUNT - start;
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
      SNAP_LOG("[api] batch [%s] httpGet failed after %d attempts\n",
               batch_label, fo.attempts_taken);
      ++summary.failed_batches;
      continue;
    }
    if (fo.attempts_taken > 1) {
      ++summary.retried_batches;
      SNAP_LOG("[api] batch [%s] succeeded on attempt %d/%d\n", batch_label,
               fo.attempts_taken, fc.max_attempts);
    }

    StreamSnapshot partial;
    if (!parseMonitorResponse(body, filters, partial)) {
      SNAP_LOG("[api] batch [%s] parse failed\n", batch_label);
      ++summary.failed_batches;
      continue;
    }

    // Copy out only the streams we asked for in this batch — other indices in
    // `partial` are default-empty by construction.
    for (int j = 0; j < batch_size; ++j) {
      int idx = FETCH_ORDER[start + j];
      out.stream[idx] = partial.stream[idx];
    }
  }

  // ÖBB S-Bahn stream: separate POST endpoint, fetched after the OGD batch so
  // an OGD failure never hides it (and vice versa).
  fetchOebbStream(net, mgate_url, oebb_filter, out, summary);

  // api_ok if at least one batch returned valid JSON. A complete network
  // failure (all batches failed) falls through to api_ok=false and
  // warmCyclePath's short-retry policy.
  out.api_ok = (summary.failed_batches < summary.total_batches);
  return out.api_ok;
}

} // namespace bustaferl
