#include "config.h"
#include "data/wienerlinien_parse.h"
#include "fixtures/wl_live.h"

#include <string>
#include <unity.h>

using namespace bustaferl;

// We embed the fixtures as string literals to keep the host build hermetic
// (no filesystem access in `pio test -e native`).

static const char *kHappyJson = R"JSON({
  "data": {
    "monitors": [
      {
        "locationStop": { "properties": { "attributes": { "rbl": 1001 } } },
        "lines": [
          {
            "name": "58A",
            "towards": "Atzgersdorf",
            "departures": {
              "departure": [
                { "departureTime": {
                    "timePlanned": "2024-01-01T12:30:00.000+0100",
                    "timeReal":    "2024-01-01T12:31:00.000+0100" } },
                { "departureTime": {
                    "timePlanned": "2024-01-01T12:45:00.000+0100",
                    "timeReal":    "2024-01-01T12:46:00.000+0100" } },
                { "departureTime": {
                    "timePlanned": "2024-01-01T13:00:00.000+0100" } }
              ]
            }
          }
        ]
      },
      {
        "locationStop": { "properties": { "attributes": { "rbl": 1002 } } },
        "lines": [
          {
            "name": "58A",
            "towards": "Hietzing",
            "departures": {
              "departure": [
                { "departureTime": {
                    "timePlanned": "2024-01-01T12:35:00.000+0100" } }
              ]
            }
          }
        ]
      },
      {
        "locationStop": { "properties": { "attributes": { "rbl": 1003 } } },
        "lines": [
          {
            "name": "58B",
            "towards": "Endemanngasse",
            "departures": { "departure": [
              { "departureTime": {
                  "timePlanned": "2024-01-01T12:32:00.000+0100" } }
            ] }
          },
          {
            "name": "58B",
            "towards": "Atzgersdorf S+U",
            "departures": { "departure": [
              { "departureTime": {
                  "timePlanned": "2024-01-01T12:40:00.000+0100",
                  "timeReal":    "2024-01-01T12:41:00.000+0100" } }
            ] }
          }
        ]
      }
    ]
  }
})JSON";

static void buildFilters(StreamFilter (&f)[STREAM_COUNT]) {
  f[STREAM_58A_ATZ] = {1001, "58A", "Atzgersdorf"};
  f[STREAM_58A_HIETZING] = {1002, "58A", "Hietzing"};
  f[STREAM_58B_ATZ] = {1003, "58B", "Atzgersdorf"};
}

void test_happy_path_all_three_streams() {
  StreamFilter f[STREAM_COUNT];
  buildFilters(f);
  StreamSnapshot s;
  TEST_ASSERT_TRUE(parseMonitorResponse(kHappyJson, f, s));
  TEST_ASSERT_TRUE(s.api_ok);

  TEST_ASSERT_TRUE(s.stream[STREAM_58A_ATZ].endpoint_responded);
  TEST_ASSERT_TRUE(s.stream[STREAM_58A_ATZ].filter_matched);
  TEST_ASSERT_TRUE(s.stream[STREAM_58A_ATZ].slot[0].valid);
  TEST_ASSERT_EQUAL(DepartureSource::Realtime,
                    s.stream[STREAM_58A_ATZ].slot[0].source);
  TEST_ASSERT_TRUE(s.stream[STREAM_58A_ATZ].slot[1].valid);
  // 12:31 CET = 11:31 UTC = 1704108660
  TEST_ASSERT_EQUAL(1704108660, s.stream[STREAM_58A_ATZ].slot[0].when);
  // Planned time kept alongside the live time so the deviation gauge can
  // render live-minus-scheduled: 12:30 CET = 1704108600, i.e. +1 min late.
  TEST_ASSERT_EQUAL(1704108600, s.stream[STREAM_58A_ATZ].slot[0].planned);
  TEST_ASSERT_TRUE(s.stream[STREAM_58A_ATZ].slot[0].hasDeviation());
  TEST_ASSERT_EQUAL(1, s.stream[STREAM_58A_ATZ].slot[0].deviationMinutes());
  // Plan-only slot (13:00, no realtime): planned recorded, but no live
  // comparison → the gauge shows the hollow "no match" square, not a bar.
  TEST_ASSERT_EQUAL(DepartureSource::Plan,
                    s.stream[STREAM_58A_ATZ].slot[2].source);
  TEST_ASSERT_FALSE(s.stream[STREAM_58A_ATZ].slot[2].hasDeviation());

  // Hietzing departure has only timePlanned → source = Plan
  TEST_ASSERT_TRUE(s.stream[STREAM_58A_HIETZING].slot[0].valid);
  TEST_ASSERT_EQUAL(DepartureSource::Plan,
                    s.stream[STREAM_58A_HIETZING].slot[0].source);

  // 58B: the "Endemanngasse" line must be skipped, "Atzgersdorf S+U" wins
  TEST_ASSERT_TRUE(s.stream[STREAM_58B_ATZ].slot[0].valid);
  TEST_ASSERT_EQUAL(DepartureSource::Realtime,
                    s.stream[STREAM_58B_ATZ].slot[0].source);
}

void test_plan_fallback_when_realtime_missing() {
  StreamFilter f[STREAM_COUNT];
  buildFilters(f);
  StreamSnapshot s;
  parseMonitorResponse(kHappyJson, f, s);
  TEST_ASSERT_TRUE(s.stream[STREAM_58A_HIETZING].slot[0].valid);
  TEST_ASSERT_EQUAL(DepartureSource::Plan,
                    s.stream[STREAM_58A_HIETZING].slot[0].source);
}

void test_filter_mismatch_marks_responded_but_not_matched() {
  const char *json = R"JSON({"data":{"monitors":[
      {"locationStop":{"properties":{"attributes":{"rbl":1003}}},
       "lines":[{"name":"58B","towards":"Endemanngasse",
                 "departures":{"departure":[]}}]}]}})JSON";
  StreamFilter f[STREAM_COUNT];
  buildFilters(f);
  StreamSnapshot s;
  TEST_ASSERT_TRUE(parseMonitorResponse(json, f, s));
  TEST_ASSERT_TRUE(s.stream[STREAM_58B_ATZ].endpoint_responded);
  TEST_ASSERT_FALSE(s.stream[STREAM_58B_ATZ].filter_matched);
}

void test_malformed_json_fails_cleanly() {
  StreamFilter f[STREAM_COUNT];
  buildFilters(f);
  StreamSnapshot s;
  TEST_ASSERT_FALSE(parseMonitorResponse("not json {{", f, s));
}

void test_empty_monitors_succeeds_with_no_data() {
  StreamFilter f[STREAM_COUNT];
  buildFilters(f);
  StreamSnapshot s;
  TEST_ASSERT_TRUE(parseMonitorResponse(R"({"data":{"monitors":[]}})", f, s));
  TEST_ASSERT_TRUE(s.api_ok);
  for (int i = 0; i < STREAM_COUNT; ++i) {
    TEST_ASSERT_FALSE(s.stream[i].endpoint_responded);
    TEST_ASSERT_FALSE(s.stream[i].slot[0].valid);
  }
}

void test_real_live_response_parses() {
  // Captured live API body (fixtures/wl_live.h). Real RBLs from config.h.
  StreamFilter f[STREAM_COUNT];
  f[STREAM_58A_ATZ] = {8131, "58A", "Atzgersdorf"};
  f[STREAM_58A_HIETZING] = {3757, "58A", "Hietzing"};
  f[STREAM_58B_ATZ] = {8132, "58B", "Atzgersdorf"};
  StreamSnapshot s;
  TEST_ASSERT_TRUE_MESSAGE(parseMonitorResponse(kLiveResponseJson, f, s),
                           "parseMonitorResponse returned false on real body");
  TEST_ASSERT_TRUE(s.api_ok);
  TEST_ASSERT_TRUE(s.stream[STREAM_58A_ATZ].endpoint_responded);
  TEST_ASSERT_TRUE(s.stream[STREAM_58A_HIETZING].endpoint_responded);
  TEST_ASSERT_TRUE(s.stream[STREAM_58B_ATZ].endpoint_responded);
}

void test_real_live_response_all_three_filters_match() {
  // Uses the production filter strings from config.h against the captured
  // live response. Locks the contract: with the real towards-prefixes, all
  // three streams must filter_match and yield a valid first slot.
  StreamFilter f[STREAM_COUNT];
  f[STREAM_58A_ATZ] = {RBL_TULL_ATZGERSDORF, LINE_58A, TOWARDS_58A_ATZ};
  f[STREAM_58A_HIETZING] = {RBL_TULL_HIETZING, LINE_58A, TOWARDS_58A_HIETZING};
  f[STREAM_58B_ATZ] = {RBL_ENDEMANN, LINE_58B, FILTER_TOWARDS_58B};
  StreamSnapshot s;
  TEST_ASSERT_TRUE(parseMonitorResponse(kLiveResponseJson, f, s));
  TEST_ASSERT_TRUE_MESSAGE(s.stream[STREAM_58A_ATZ].filter_matched,
                           "58A→Atzgersdorf filter did not match real data");
  TEST_ASSERT_TRUE_MESSAGE(s.stream[STREAM_58A_HIETZING].filter_matched,
                           "58A→Hietzing filter did not match real data");
  TEST_ASSERT_TRUE_MESSAGE(s.stream[STREAM_58B_ATZ].filter_matched,
                           "58B→Atzgersdorf filter did not match real data");
  TEST_ASSERT_TRUE(s.stream[STREAM_58A_ATZ].slot[0].valid);
  TEST_ASSERT_TRUE(s.stream[STREAM_58A_HIETZING].slot[0].valid);
  TEST_ASSERT_TRUE(s.stream[STREAM_58B_ATZ].slot[0].valid);
}

void setUp() {}
void tearDown() {}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_happy_path_all_three_streams);
  RUN_TEST(test_plan_fallback_when_realtime_missing);
  RUN_TEST(test_filter_mismatch_marks_responded_but_not_matched);
  RUN_TEST(test_malformed_json_fails_cleanly);
  RUN_TEST(test_empty_monitors_succeeds_with_no_data);
  RUN_TEST(test_real_live_response_parses);
  RUN_TEST(test_real_live_response_all_three_filters_match);
  return UNITY_END();
}
