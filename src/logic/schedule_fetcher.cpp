#include "schedule_fetcher.h"

#include "../data/time_constants.h"
#include "api_fetcher.h"

#include <cstdint>
#include <cstdio>

#ifndef NATIVE_BUILD
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#define SCHED_LOG(...) Serial.printf(__VA_ARGS__)
// EFA + TLS peaks ~90 KB above the idle baseline. Bail before the call
// if free heap is below this — better to skip a refresh than to crash
// and bootloop.
#define SCHED_MIN_FREE_HEAP 90000u
// The TLS handshake also needs at least one ~50 KB contiguous block for
// the mbedtls session buffer. After repeated calls the heap can have
// 200 KB free but be too fragmented to hand one out; without this check
// the PHY allocator hit that case and aborted (no recoverable error
// path inside libphy.a).
#define SCHED_MIN_LARGEST_BLOCK 60000u
static inline uint32_t freeHeap() { return esp_get_free_heap_size(); }
static inline uint32_t largestFreeBlock() {
  return heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);
}
#else
#define SCHED_LOG(...) ((void)0)
// On native there is no real heap to bail on; the probes return
// UINT32_MAX so the threshold checks are always false. Using 1u rather
// than 0u avoids gcc's tautological-compare warning on `unsigned < 0`.
#define SCHED_MIN_FREE_HEAP 1u
#define SCHED_MIN_LARGEST_BLOCK 1u
static inline uint32_t freeHeap() { return UINT32_MAX; }
static inline uint32_t largestFreeBlock() { return UINT32_MAX; }
#endif

namespace bustaferl {

namespace {

// Returns true iff DIVA `d` appears in filters[0..i-1] — used to skip
// duplicate Haltestellen (Südtiroler Platz is shared by both U1 streams).
bool isDuplicateDiva(const ScheduleStreamFilter (&filters)[STREAM_COUNT],
                     int up_to, int d) {
  for (int j = 0; j < up_to; ++j) {
    if (filters[j].diva == d)
      return true;
  }
  return false;
}

} // namespace

std::string buildEfaUrl(const std::string &base, int diva, time_t query_time,
                        int limit) {
  struct tm local;
  localtime_r(&query_time, &local);
  // 160 byte buffer: WL_EFA_DM_BASE (~80) + 8 numeric params + format glue —
  // worst case ~140 bytes; 160 leaves margin for sane base URLs.
  constexpr size_t EFA_URL_BUF_SIZE = 160;
  char buf[EFA_URL_BUF_SIZE];
  std::snprintf(buf, sizeof(buf),
                "%s&name_dm=%d&itdDateDay=%02d&itdDateMonth=%02d"
                "&itdDateYear=%04d&itdTimeHour=%02d&itdTimeMinute=%02d"
                "&limit=%d",
                base.c_str(), diva, local.tm_mday, local.tm_mon + 1,
                local.tm_year + TM_YEAR_BASE, local.tm_hour, local.tm_min,
                limit);
  return std::string(buf);
}

time_t computeCutoff(time_t now, int cutoff_hour) {
  struct tm local;
  localtime_r(&now, &local);
  // Move to "tomorrow at cutoff_hour:00". If now is already past today's
  // cutoff_hour we still want the *next* one, so unconditionally +1 day.
  local.tm_mday += 1;
  local.tm_hour = cutoff_hour;
  local.tm_min = 0;
  local.tm_sec = 0;
  local.tm_isdst = -1;
  return mktime(&local);
}

// Inlines the heap-guard + delay() interleaving that protects against ESP32
// TLS/mbedtls fragmentation (see docs/main-refactor-plan.md §7.1
// "Heap-Wächter"). Splitting the iteration would either duplicate the guard
// or hide it behind a callback indirection. The post-refactor TODO §7.1
// calls out re-evaluating the guards once the native-runtime exists; this
// NOLINT lifts together with that.
// NOLINTBEGIN(readability-function-size)
ScheduleFetchResult
fetchSchedule(INetwork &net, time_t now,
              const ScheduleStreamFilter (&filters)[STREAM_COUNT],
              const ScheduleFetchConfig &cfg) {
  ScheduleFetchResult out;

  // Anchor query at today at query_hour:query_minute local. EFA returns
  // departures from that point forward, which spans the evening + next
  // morning we care about.
  time_t query_time = 0;
  {
    struct tm local;
    localtime_r(&now, &local);
    local.tm_hour = cfg.query_hour;
    local.tm_min = cfg.query_minute;
    local.tm_sec = 0;
    local.tm_isdst = -1;
    query_time = mktime(&local);
  }
  time_t cutoff = computeCutoff(now, cfg.cutoff_hour);

  for (int i = 0; i < STREAM_COUNT; ++i) {
    int diva = filters[i].diva;
    if (diva == 0)
      continue;
    if (isDuplicateDiva(filters, i, diva))
      continue;

    uint32_t heap_before = freeHeap();
    uint32_t largest_before = largestFreeBlock();
    if (heap_before < SCHED_MIN_FREE_HEAP ||
        largest_before < SCHED_MIN_LARGEST_BLOCK) {
      SCHED_LOG("[sched] diva=%d skipped, free=%u largest=%u (need %u/%u)\n",
                diva, static_cast<unsigned>(heap_before),
                static_cast<unsigned>(largest_before),
                static_cast<unsigned>(SCHED_MIN_FREE_HEAP),
                static_cast<unsigned>(SCHED_MIN_LARGEST_BLOCK));
      ++out.calls_attempted;
      ++out.calls_failed;
      continue;
    }

    std::string url =
        buildEfaUrl(cfg.endpoint_base, diva, query_time, cfg.limit);
    ++out.calls_attempted;

#ifndef NATIVE_BUILD
    // Stream-parse path: ArduinoJson reads bytes directly off the HTTP
    // stream, so the 38 KB body is never buffered. Cuts peak heap by
    // roughly that amount, which is what keeps three back-to-back TLS
    // sessions from tipping mbedtls/PHY into an abort.
    bool parsed = false;
    bool got = net.httpGetStream(url, [&](::Stream &s) {
      parsed = parseEfaResponse(s, diva, filters, cutoff, out.hint);
      return parsed;
    });
    if (!got) {
      SCHED_LOG("[sched] diva=%d %s\n", diva,
                parsed ? "httpGetStream failed" : "parse failed");
      ++out.calls_failed;
      continue;
    }
#else
    {
      std::string body;
      FetchConfig fc;
      FetchOutcome fo = fetchWithRetry(net, url, body, fc);
      if (!fo.ok) {
        SCHED_LOG("[sched] diva=%d httpGet failed after %d attempts\n", diva,
                  fo.attempts_taken);
        ++out.calls_failed;
        continue;
      }
      if (!parseEfaResponse(body, diva, filters, cutoff, out.hint)) {
        SCHED_LOG("[sched] diva=%d parse failed (%u bytes)\n", diva,
                  static_cast<unsigned>(body.size()));
        ++out.calls_failed;
        continue;
      }
    }
#endif
    SCHED_LOG("[sched] diva=%d ok, heap %u -> %u\n", diva,
              static_cast<unsigned>(heap_before),
              static_cast<unsigned>(freeHeap()));
#ifndef NATIVE_BUILD
    // Give the WiFi/mbedtls allocator a moment to actually reclaim TLS
    // context memory before the next HTTPS handshake — otherwise the third
    // back-to-back call faced a fragmented heap with min_free < 50 KB and
    // mbedtls threw std::bad_alloc inside abort().
    delay(150);
#endif
  }

  out.ok = out.calls_attempted > 0 && out.calls_failed < out.calls_attempted;
  return out;
}
// NOLINTEND(readability-function-size)

} // namespace bustaferl
