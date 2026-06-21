// Host tests for snapshot_logger: formatSlot + formatSnapshotSummary. Pin the
// Serial-log format byte-for-byte so future touch-ups can't drift silently.
// The history (pre-extraction main.cpp) printed the summary header with an
// inconsistent "58B" abbreviation while per-slot lines used "58B-Atz[0]"; the
// extraction unifies both to "58B-Atz" via streamLabel(). That is the only
// intentional drift from the Schritt-0b baseline.

#include "data/Departure.h"
#include "data/StreamSnapshot.h"
#include "logic/snapshot_logger.h"

#include <cstring>
#include <string>
#include <unity.h>

using namespace bustaferl;

void test_source_tag_known_values() {
  TEST_ASSERT_EQUAL_STRING("RT", sourceTag(DepartureSource::Realtime));
  TEST_ASSERT_EQUAL_STRING("PLAN", sourceTag(DepartureSource::Plan));
  TEST_ASSERT_EQUAL_STRING("HINT", sourceTag(DepartureSource::Hint));
  TEST_ASSERT_EQUAL_STRING("??", sourceTag(DepartureSource::Unknown));
}

void test_format_slot_invalid_is_dashes() {
  Departure d{};
  // Default-constructed Departure: valid=false. Format must be the literal
  // "[api]   <tag>: --:--\n".
  std::string out = formatSlot("X", d);
  TEST_ASSERT_EQUAL_STRING("[api]   X: --:--\n", out.c_str());
}

void test_format_slot_valid_uses_local_time_and_source() {
  // Fixed UTC epoch — set TZ to UTC for determinism; localtime_r will then
  // produce 06:30 for 1700000000 + (6*3600 + 30*60 - some offset).
  // Pick an epoch we can hand-verify: 2024-01-01 12:34:00 UTC = 1704112440.
  setenv("TZ", "UTC", 1);
  tzset();
  Departure d{};
  d.valid = true;
  d.when = 1704112440;
  d.source = DepartureSource::Realtime;

  std::string out = formatSlot("58A-Atz[0]", d);
  TEST_ASSERT_EQUAL_STRING("[api]   58A-Atz[0]: 12:34 RT epoch=1704112440\n",
                           out.c_str());
}

void test_format_slot_hint_tag() {
  setenv("TZ", "UTC", 1);
  tzset();
  Departure d{};
  d.valid = true;
  d.when = 1704112440;
  d.source = DepartureSource::Hint;
  std::string out = formatSlot("SBahn-Hbf[1]", d);
  TEST_ASSERT_EQUAL_STRING(
      "[api]   SBahn-Hbf[1]: 12:34 HINT epoch=1704112440\n", out.c_str());
}

void test_format_summary_header_and_per_slot_lines() {
  setenv("TZ", "UTC", 1);
  tzset();

  StreamSnapshot snap;
  snap.api_ok = true;
  // Mark every stream as responded + matched, but leave most slots invalid.
  for (int i = 0; i < STREAM_COUNT; ++i) {
    snap.stream[i].endpoint_responded = true;
    snap.stream[i].filter_matched = true;
  }
  // One concrete slot so we can verify the line shape end-to-end.
  snap.stream[STREAM_58A_ATZ].slot[0].valid = true;
  snap.stream[STREAM_58A_ATZ].slot[0].when = 1704112440;
  snap.stream[STREAM_58A_ATZ].slot[0].source = DepartureSource::Plan;

  std::string out = formatSnapshotSummary(snap, /*total_batches=*/3,
                                          /*failed_batches=*/0);

  // Header line — leading shape unchanged from the historic main.cpp print,
  // except the "58B" stream now reads as "58B-Atz" (intentional, see file
  // header comment).
  TEST_ASSERT_NOT_NULL(std::strstr(
      out.c_str(),
      "[api] batches=3 failed=0 api_ok=1  streams: 58A-Atz r=1 f=1 | "
      "58A-Hie r=1 f=1 | 58B-Atz r=1 f=1 | SBahn-Hbf r=1 f=1\n"));

  // Per-slot line for the valid Plan departure.
  TEST_ASSERT_NOT_NULL(std::strstr(
      out.c_str(), "[api]   58A-Atz[0]: 12:34 PLAN epoch=1704112440\n"));

  // Every other slot must show "--:--". Sample one from each remaining stream
  // to be sure the loop ran across all four streams.
  TEST_ASSERT_NOT_NULL(std::strstr(out.c_str(), "[api]   58A-Atz[1]: --:--\n"));
  TEST_ASSERT_NOT_NULL(std::strstr(out.c_str(), "[api]   58A-Hie[0]: --:--\n"));
  TEST_ASSERT_NOT_NULL(std::strstr(out.c_str(), "[api]   58B-Atz[0]: --:--\n"));
  TEST_ASSERT_NOT_NULL(
      std::strstr(out.c_str(), "[api]   SBahn-Hbf[0]: --:--\n"));
  TEST_ASSERT_NOT_NULL(
      std::strstr(out.c_str(), "[api]   SBahn-Hbf[1]: --:--\n"));
}

void test_format_summary_api_ok_false_and_failed_counts() {
  StreamSnapshot snap;
  snap.api_ok = false;
  std::string out = formatSnapshotSummary(snap, 3, 3);
  TEST_ASSERT_NOT_NULL(
      std::strstr(out.c_str(), "[api] batches=3 failed=3 api_ok=0  streams:"));
}

void setUp() {}
void tearDown() {}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_source_tag_known_values);
  RUN_TEST(test_format_slot_invalid_is_dashes);
  RUN_TEST(test_format_slot_valid_uses_local_time_and_source);
  RUN_TEST(test_format_slot_hint_tag);
  RUN_TEST(test_format_summary_header_and_per_slot_lines);
  RUN_TEST(test_format_summary_api_ok_false_and_failed_counts);
  return UNITY_END();
}
