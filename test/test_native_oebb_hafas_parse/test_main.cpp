// Host tests for the ÖBB HAFAS (mgate.exe) StationBoard parser: request-body
// composition + response parsing into a StreamData (line labels, realtime vs
// plan, cancelled skip, err handling, label abbreviation). Times are local
// Vienna, interpreted via mktime with $TZ set in setUp (same contract as
// efa_parse).

#include "data/oebb_hafas_parse.h"

#include <string>
#include <unity.h>

using namespace bustaferl;

namespace {

OebbStreamFilter filter() {
  return OebbStreamFilter{"8100634", "8100002", "63", 6};
}

bool contains(const std::string &hay, const char *needle) {
  return hay.find(needle) != std::string::npos;
}

// Two S-Bahn departures (S2 12:06, S3 12:12), plan only.
const char *kTwoDeps = R"JSON({
  "svcResL": [{ "res": {
    "common": { "prodL": [{ "name": "S2" }, { "name": "S3" }] },
    "jnyL": [
      { "stbStop": { "dDateS": "20240101", "dTimeS": "120600" }, "prodX": 0 },
      { "stbStop": { "dDateS": "20240101", "dTimeS": "121200" }, "prodX": 1 }
    ]
  } }],
  "err": "OK"
})JSON";

} // namespace

void test_build_request_includes_aid_client_and_filter() {
  std::string body = buildOebbRequest(filter());
  TEST_ASSERT_TRUE_MESSAGE(contains(body, "\"aid\""), "missing aid");
  TEST_ASSERT_TRUE_MESSAGE(contains(body, "vs_webapp"), "missing client frag");
  TEST_ASSERT_TRUE_MESSAGE(contains(body, "StationBoard"), "missing meth");
  TEST_ASSERT_TRUE_MESSAGE(contains(body, "\"extId\":\"8100634\""),
                           "missing stbLoc EVA");
  TEST_ASSERT_TRUE_MESSAGE(contains(body, "\"extId\":\"8100002\""),
                           "missing dirLoc EVA");
  TEST_ASSERT_TRUE_MESSAGE(contains(body, "\"value\":\"63\""),
                           "missing product filter");
  TEST_ASSERT_TRUE_MESSAGE(contains(body, "\"maxJny\":6"), "missing maxJny");
}

void test_parse_two_departures_with_labels() {
  StreamData s;
  TEST_ASSERT_TRUE(parseOebbStationBoard(kTwoDeps, s));
  TEST_ASSERT_TRUE(s.endpoint_responded);
  TEST_ASSERT_TRUE(s.filter_matched);

  TEST_ASSERT_TRUE(s.slot[0].valid);
  TEST_ASSERT_EQUAL_STRING("S2", s.slot[0].line_label);
  TEST_ASSERT_EQUAL(DepartureSource::Plan, s.slot[0].source);
  // 12:06 CET = 11:06 UTC = 1704107160
  TEST_ASSERT_EQUAL_INT64(1704107160, s.slot[0].when);

  TEST_ASSERT_TRUE(s.slot[1].valid);
  TEST_ASSERT_EQUAL_STRING("S3", s.slot[1].line_label);
  // 12:12 CET = 11:12 UTC = 1704107520
  TEST_ASSERT_EQUAL_INT64(1704107520, s.slot[1].when);
}

void test_parse_realtime_overrides_plan_and_falls_back_to_plan_date() {
  // dTimeR present, dDateR absent → realtime time on the plan date.
  const char *json = R"JSON({
    "svcResL": [{ "res": {
      "common": { "prodL": [{ "name": "S2" }] },
      "jnyL": [
        { "stbStop": { "dDateS": "20240101", "dTimeS": "120600",
                       "dTimeR": "120900" }, "prodX": 0 }
      ]
    } }],
    "err": "OK"
  })JSON";
  StreamData s;
  TEST_ASSERT_TRUE(parseOebbStationBoard(json, s));
  TEST_ASSERT_TRUE(s.slot[0].valid);
  TEST_ASSERT_EQUAL(DepartureSource::Realtime, s.slot[0].source);
  // 12:09 CET = 11:09 UTC = 1704107340
  TEST_ASSERT_EQUAL_INT64(1704107340, s.slot[0].when);
}

void test_parse_cancelled_skipped() {
  const char *json = R"JSON({
    "svcResL": [{ "res": {
      "common": { "prodL": [{ "name": "S2" }] },
      "jnyL": [
        { "stbStop": { "dDateS": "20240101", "dTimeS": "120600",
                       "dCncl": true }, "prodX": 0 },
        { "stbStop": { "dDateS": "20240101", "dTimeS": "121200" }, "prodX": 0 }
      ]
    } }],
    "err": "OK"
  })JSON";
  StreamData s;
  TEST_ASSERT_TRUE(parseOebbStationBoard(json, s));
  TEST_ASSERT_TRUE(s.slot[0].valid);
  TEST_ASSERT_EQUAL_INT64(1704107520, s.slot[0].when); // the 12:12, not 12:06
  TEST_ASSERT_FALSE(s.slot[1].valid);
}

void test_parse_err_not_ok_sets_not_responded() {
  StreamData s;
  TEST_ASSERT_TRUE(parseOebbStationBoard(R"({"err":"AID"})", s));
  TEST_ASSERT_FALSE(s.endpoint_responded);
  TEST_ASSERT_FALSE(s.filter_matched);
}

void test_parse_empty_jnyL_responded_not_matched() {
  StreamData s;
  TEST_ASSERT_TRUE(parseOebbStationBoard(
      R"({"svcResL":[{"res":{"jnyL":[]}}],"err":"OK"})", s));
  TEST_ASSERT_TRUE(s.endpoint_responded);
  TEST_ASSERT_FALSE(s.filter_matched);
}

void test_parse_long_label_abbreviated_to_xx() {
  const char *json = R"JSON({
    "svcResL": [{ "res": {
      "common": { "prodL": [{ "name": "Nightjet 456" }] },
      "jnyL": [
        { "stbStop": { "dDateS": "20240101", "dTimeS": "120600" }, "prodX": 0 }
      ]
    } }],
    "err": "OK"
  })JSON";
  StreamData s;
  TEST_ASSERT_TRUE(parseOebbStationBoard(json, s));
  TEST_ASSERT_TRUE(s.slot[0].valid);
  TEST_ASSERT_EQUAL_STRING("xx", s.slot[0].line_label);
}

void test_malformed_json_fails_cleanly() {
  StreamData s;
  TEST_ASSERT_FALSE(parseOebbStationBoard("not json {{", s));
}

void setUp() {
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();
}
void tearDown() {}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_build_request_includes_aid_client_and_filter);
  RUN_TEST(test_parse_two_departures_with_labels);
  RUN_TEST(test_parse_realtime_overrides_plan_and_falls_back_to_plan_date);
  RUN_TEST(test_parse_cancelled_skipped);
  RUN_TEST(test_parse_err_not_ok_sets_not_responded);
  RUN_TEST(test_parse_empty_jnyL_responded_not_matched);
  RUN_TEST(test_parse_long_label_abbreviated_to_xx);
  RUN_TEST(test_malformed_json_fails_cleanly);
  return UNITY_END();
}
