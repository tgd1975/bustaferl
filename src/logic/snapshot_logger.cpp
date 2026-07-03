#include "logic/snapshot_logger.h"

#include "data/stream_labels.h"

#include <cstdio>
#include <ctime>

namespace bustaferl {

namespace {

// Per-slot line ("[api]   <tag>: HH:MM <SRC> epoch=<ts>\n") fits well under
// 96 bytes for any plausible tag/epoch combination — leave headroom for the
// 19-char epoch and the 10-char tag.
constexpr int SLOT_LINE_BUF = 96;
// One "<label> r=N f=N |" segment per stream — labels max 8 chars + 16
// fixed chars + null.
constexpr int SUMMARY_SEG_BUF = 48;
// Header up to "streams:" ends in <50 chars for any sane batch counter.
constexpr int SUMMARY_HEAD_BUF = 256;
// Tag for the per-slot lines: "<label>[%d]", labels up to 8 chars.
constexpr int SLOT_TAG_BUF = 24;
// Snapshot summary buffer reservation hint.
constexpr int SUMMARY_RESERVE_BYTES = 512;

} // namespace

const char *sourceTag(DepartureSource s) {
  switch (s) {
  case DepartureSource::Realtime:
    return "RT";
  case DepartureSource::Plan:
    return "PLAN";
  case DepartureSource::Hint:
    return "HINT";
  case DepartureSource::Unknown:
  default:
    return "??";
  }
}

std::string formatSlot(const char *tag, const Departure &d) {
  char buf[SLOT_LINE_BUF];
  if (!d.valid) {
    std::snprintf(buf, sizeof(buf), "[api]   %s: --:--\n", tag);
    return std::string{buf};
  }
  struct tm local {};
  localtime_r(&d.when, &local);
  std::snprintf(buf, sizeof(buf), "[api]   %s: %02d:%02d %s epoch=%lld\n", tag,
                local.tm_hour, local.tm_min, sourceTag(d.source),
                static_cast<long long>(d.when));
  return std::string{buf};
}

std::string formatSnapshotSummary(const StreamSnapshot &snap, int total_batches,
                                  int failed_batches) {
  std::string out;
  out.reserve(SUMMARY_RESERVE_BYTES);

  // Header line: identical wording to the historic main.cpp variant, but
  // built from a per-stream loop instead of 10 hard-coded format args so
  // the stream labels live exactly once.
  char head[SUMMARY_HEAD_BUF];
  std::snprintf(head, sizeof(head),
                "[api] batches=%d failed=%d api_ok=%d  streams:", total_batches,
                failed_batches, snap.api_ok ? 1 : 0);
  out += head;
  for (int i = 0; i < STREAM_COUNT; ++i) {
    char seg[SUMMARY_SEG_BUF];
    std::snprintf(seg, sizeof(seg), " %s r=%d f=%d%s", streamLabel(i),
                  snap.stream[i].endpoint_responded ? 1 : 0,
                  snap.stream[i].filter_matched ? 1 : 0,
                  (i + 1 == STREAM_COUNT) ? "" : " |");
    out += seg;
  }
  out += '\n';

  for (int i = 0; i < STREAM_COUNT; ++i) {
    for (int j = 0; j < SLOTS_PER_STREAM; ++j) {
      char tag[SLOT_TAG_BUF];
      std::snprintf(tag, sizeof(tag), "%s[%d]", streamLabel(i), j);
      out += formatSlot(tag, snap.stream[i].slot[j]);
    }
  }
  return out;
}

} // namespace bustaferl
