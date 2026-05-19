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

const int FETCH_ORDER[STREAM_COUNT] = {
    STREAM_U1_OBERLAA,   STREAM_U1_LEOPOLDAU, STREAM_58B_ATZ,
    STREAM_58A_HIETZING, STREAM_58A_ATZ,
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

bool fetchSnapshot(INetwork &net, const std::string &endpoint_base,
                   const StreamFilter (&filters)[STREAM_COUNT],
                   StreamSnapshot &out, FetchSummary &summary) {
  out = StreamSnapshot{};
  summary = FetchSummary{};

  for (int start = 0; start < STREAM_COUNT; start += STOPIDS_PER_QUERY) {
    int batch_size = STREAM_COUNT - start;
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

  // api_ok if at least one batch returned valid JSON. A complete network
  // failure (all batches failed httpGet/parse) falls through to api_ok=false
  // and warmCyclePath's short-retry policy.
  out.api_ok = (summary.failed_batches < summary.total_batches);
  return out.api_ok;
}

} // namespace bustaferl
