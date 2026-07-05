#include "config.h"
#include "data/oebb_hafas_parse.h"
#include "fixtures/oebb_live.h"

#include <ArduinoJson.h>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unity.h>

using namespace bustaferl;

// HAFAS-Zeitstempel sind lokale Europe/Vienna-Zeit ohne TZ-Suffix; mktime
// braucht $TZ. Auf dem Device setzt Esp32Clock::setEnvTz; im Host-Test
// machen wir das einmal in setUp().
static void setViennaTz() {
  setenv("TZ", TZ_INFO, 1);
  tzset();
}

void setUp() { setViennaTz(); }
void tearDown() {}

// Parse with no past-skip: every fixture departure is in 2026, so a now of
// epoch 0 keeps them all and preserves the pre-past-skip behaviour these
// label/epoch tests assert. The dedicated skip test below uses a real now.
static const time_t kNoFilter = 0;

// ---- buildOebbRequest --------------------------------------------------

static OebbStreamFilter makeFilter() {
  OebbStreamFilter f;
  f.stbloc_extid = OEBB_STBLOC_EXTID;
  f.dirloc_extid = OEBB_DIRLOC_EXTID;
  f.products = OEBB_JNYFLTR_PRODUCTS;
  f.max_jny = OEBB_MAX_JNY;
  return f;
}

void test_buildRequest_includes_aid_and_client() {
  std::string body = buildOebbRequest(makeFilter());
  TEST_ASSERT_NOT_EQUAL(std::string::npos,
                        body.find("\"aid\":\"" OEBB_HAFAS_AID "\""));
  TEST_ASSERT_NOT_EQUAL(std::string::npos, body.find("\"client\""));
  TEST_ASSERT_NOT_EQUAL(std::string::npos, body.find("\"webapp\""));
  TEST_ASSERT_NOT_EQUAL(std::string::npos, body.find("\"vs_webapp\""));
  TEST_ASSERT_NOT_EQUAL(std::string::npos,
                        body.find("\"ver\":\"" OEBB_HAFAS_VER "\""));
}

void test_buildRequest_includes_stbLoc_dirLoc() {
  std::string body = buildOebbRequest(makeFilter());
  TEST_ASSERT_NOT_EQUAL(std::string::npos, body.find("\"stbLoc\""));
  TEST_ASSERT_NOT_EQUAL(std::string::npos,
                        body.find("\"extId\":\"" OEBB_EXTID_ATZG "\""));
  TEST_ASSERT_NOT_EQUAL(std::string::npos, body.find("\"dirLoc\""));
  TEST_ASSERT_NOT_EQUAL(std::string::npos,
                        body.find("\"extId\":\"" OEBB_EXTID_WIENHBF "\""));
  // Method must be StationBoard (DEP).
  TEST_ASSERT_NOT_EQUAL(std::string::npos,
                        body.find("\"meth\":\"StationBoard\""));
  TEST_ASSERT_NOT_EQUAL(std::string::npos, body.find("\"type\":\"DEP\""));
}

void test_buildRequest_includes_products_filter() {
  std::string body = buildOebbRequest(makeFilter());
  TEST_ASSERT_NOT_EQUAL(std::string::npos, body.find("\"jnyFltrL\""));
  TEST_ASSERT_NOT_EQUAL(std::string::npos, body.find("\"type\":\"PROD\""));
  TEST_ASSERT_NOT_EQUAL(std::string::npos, body.find("\"mode\":\"INC\""));
  TEST_ASSERT_NOT_EQUAL(std::string::npos,
                        body.find("\"value\":\"" OEBB_JNYFLTR_PRODUCTS "\""));
  TEST_ASSERT_NOT_EQUAL(std::string::npos, body.find("\"maxJny\":6"));
}

void test_buildRequest_omits_cfg_block() {
  // Pre-Phase 2026-05-19: `cfg` führt zu err=PARSE — Body darf den Block
  // also nicht enthalten.
  std::string body = buildOebbRequest(makeFilter());
  TEST_ASSERT_EQUAL(std::string::npos, body.find("\"cfg\""));
}

// ---- parseOebbStationBoard, Live-Fixtures ------------------------------

void test_parse_sample1_two_slots_realtime_S2() {
  StreamData out;
  OebbParseResult r;
  TEST_ASSERT_TRUE(parseOebbStationBoard(kSample1Json, kNoFilter, out, r));
  TEST_ASSERT_TRUE(r.endpoint_responded);
  TEST_ASSERT_TRUE(r.filter_matched);
  TEST_ASSERT_FALSE(r.auth_error_seen);

  TEST_ASSERT_TRUE(out.slot[0].valid);
  TEST_ASSERT_TRUE(out.slot[1].valid);
  // Sample-1 first jny: nameS="S 2", dTimeR=143200 (pünktlich → Realtime).
  TEST_ASSERT_EQUAL_STRING("S2", out.slot[0].line_label);
  TEST_ASSERT_EQUAL(DepartureSource::Realtime, out.slot[0].source);
  // Second: nameS="S 4", dTimeR=144400.
  TEST_ASSERT_EQUAL_STRING("S4", out.slot[1].line_label);
  TEST_ASSERT_EQUAL(DepartureSource::Realtime, out.slot[1].source);
  // Departures must be strictly increasing in time.
  TEST_ASSERT_LESS_THAN(out.slot[1].when, out.slot[0].when);
}

void test_parse_sample2_line_labels_S1_S2() {
  StreamData out;
  OebbParseResult r;
  TEST_ASSERT_TRUE(parseOebbStationBoard(kSample2Json, kNoFilter, out, r));
  TEST_ASSERT_TRUE(r.endpoint_responded);
  TEST_ASSERT_TRUE(r.filter_matched);
  // First: S1, second: S2.
  TEST_ASSERT_EQUAL_STRING("S1", out.slot[0].line_label);
  TEST_ASSERT_EQUAL_STRING("S2", out.slot[1].line_label);
}

void test_parse_sample3_line_labels_S2_S3() {
  StreamData out;
  OebbParseResult r;
  TEST_ASSERT_TRUE(parseOebbStationBoard(kSample3Json, kNoFilter, out, r));
  TEST_ASSERT_TRUE(r.endpoint_responded);
  TEST_ASSERT_TRUE(r.filter_matched);
  TEST_ASSERT_EQUAL_STRING("S2", out.slot[0].line_label);
  TEST_ASSERT_EQUAL_STRING("S3", out.slot[1].line_label);
}

void test_parse_sample1_absolute_epoch_via_TZ() {
  // 2026-05-19 14:32:00 Europe/Vienna (CEST = UTC+2) = 12:32:00 UTC →
  // epoch 1779193920. Locks the local-time → epoch path against silent TZ
  // regressions (forgotten setenv("TZ", …), missing tzset(), etc.).
  StreamData out;
  OebbParseResult r;
  TEST_ASSERT_TRUE(parseOebbStationBoard(kSample1Json, kNoFilter, out, r));
  TEST_ASSERT_EQUAL_INT64(1779193920, out.slot[0].when);
}

// ---- parseOebbStationBoard, synthetic error paths -----------------------

void test_parse_cancelled_skipped() {
  // Cancelled jny first, valid jny second → only second lands in slot[0].
  static const char *kJson = R"JSON({
    "err": "OK",
    "svcResL": [{
      "res": {
        "common": { "prodL": [
          { "nameS": "S 1" },
          { "nameS": "S 2" }
        ] },
        "jnyL": [
          {
            "date": "20260519",
            "prodX": 0,
            "stbStop": {
              "dCncl": true,
              "dTimeS": "143200",
              "dTimeR": "143200"
            }
          },
          {
            "date": "20260519",
            "prodX": 1,
            "stbStop": {
              "dTimeS": "143700",
              "dTimeR": "143700"
            }
          }
        ]
      }
    }]
  })JSON";
  StreamData out;
  OebbParseResult r;
  TEST_ASSERT_TRUE(parseOebbStationBoard(kJson, kNoFilter, out, r));
  TEST_ASSERT_TRUE(r.endpoint_responded);
  TEST_ASSERT_TRUE(r.filter_matched);
  TEST_ASSERT_TRUE(out.slot[0].valid);
  TEST_ASSERT_EQUAL_STRING("S2", out.slot[0].line_label);
  TEST_ASSERT_FALSE(out.slot[1].valid);
}

void test_parse_plan_only_falls_back_to_Plan_source() {
  static const char *kJson = R"JSON({
    "err": "OK",
    "svcResL": [{
      "res": {
        "common": { "prodL": [ { "nameS": "S 1" } ] },
        "jnyL": [{
          "date": "20260519",
          "prodX": 0,
          "stbStop": { "dTimeS": "143200" }
        }]
      }
    }]
  })JSON";
  StreamData out;
  OebbParseResult r;
  TEST_ASSERT_TRUE(parseOebbStationBoard(kJson, kNoFilter, out, r));
  TEST_ASSERT_TRUE(out.slot[0].valid);
  TEST_ASSERT_EQUAL(DepartureSource::Plan, out.slot[0].source);
}

void test_parse_err_AID_sets_auth_error_seen() {
  // err="AID" → Auth-Pfad, KEIN endpoint_responded.
  const char *kJson = R"JSON({ "err": "AID" })JSON";
  StreamData out;
  OebbParseResult r;
  TEST_ASSERT_TRUE(parseOebbStationBoard(kJson, kNoFilter, out, r));
  TEST_ASSERT_TRUE(r.auth_error_seen);
  TEST_ASSERT_FALSE(r.endpoint_responded);
  TEST_ASSERT_FALSE(r.filter_matched);
}

void test_parse_err_AUTH_sets_auth_error_seen() {
  const char *kJson = R"JSON({ "err": "AUTH" })JSON";
  StreamData out;
  OebbParseResult r;
  TEST_ASSERT_TRUE(parseOebbStationBoard(kJson, kNoFilter, out, r));
  TEST_ASSERT_TRUE(r.auth_error_seen);
  TEST_ASSERT_FALSE(r.endpoint_responded);
}

void test_parse_err_FAIL_sets_endpoint_not_responded() {
  // err="FAIL" → Stale/Offline-Pfad, kein Auth.
  const char *kJson = R"JSON({ "err": "FAIL" })JSON";
  StreamData out;
  OebbParseResult r;
  TEST_ASSERT_TRUE(parseOebbStationBoard(kJson, kNoFilter, out, r));
  TEST_ASSERT_FALSE(r.auth_error_seen);
  TEST_ASSERT_FALSE(r.endpoint_responded);
  TEST_ASSERT_FALSE(r.filter_matched);
}

void test_parse_empty_jnyL_sets_filter_unmatched() {
  const char *kJson = R"JSON({
    "err": "OK",
    "svcResL": [{ "res": { "common": { "prodL": [] }, "jnyL": [] } }]
  })JSON";
  StreamData out;
  OebbParseResult r;
  TEST_ASSERT_TRUE(parseOebbStationBoard(kJson, kNoFilter, out, r));
  TEST_ASSERT_TRUE(r.endpoint_responded);
  TEST_ASSERT_FALSE(r.filter_matched);
}

void test_parse_long_line_label_abbreviates_xx() {
  // 7-char nameS, stripped länger als LINE_LABEL_CAP-1 (5) → "xx".
  const char *kJson = R"JSON({
    "err": "OK",
    "svcResL": [{
      "res": {
        "common": { "prodL": [ { "nameS": "ABCDEFG" } ] },
        "jnyL": [{
          "date": "20260519",
          "prodX": 0,
          "stbStop": { "dTimeS": "143200" }
        }]
      }
    }]
  })JSON";
  StreamData out;
  OebbParseResult r;
  TEST_ASSERT_TRUE(parseOebbStationBoard(kJson, kNoFilter, out, r));
  TEST_ASSERT_TRUE(out.slot[0].valid);
  TEST_ASSERT_EQUAL_STRING("xx", out.slot[0].line_label);
}

void test_parse_REX_label_strips_to_REX1() {
  // Boundary: "REX 1" (5 chars stripped) muss noch passen, nicht zu xx.
  const char *kJson = R"JSON({
    "err": "OK",
    "svcResL": [{
      "res": {
        "common": { "prodL": [ { "nameS": "REX 1" } ] },
        "jnyL": [{
          "date": "20260519",
          "prodX": 0,
          "stbStop": { "dTimeS": "143200" }
        }]
      }
    }]
  })JSON";
  StreamData out;
  OebbParseResult r;
  TEST_ASSERT_TRUE(parseOebbStationBoard(kJson, kNoFilter, out, r));
  TEST_ASSERT_TRUE(out.slot[0].valid);
  TEST_ASSERT_EQUAL_STRING("REX1", out.slot[0].line_label);
}

void test_parse_skips_departed_journeys() {
  // HAFAS lists the just-departed train first. With `now` past the 14:32 train,
  // the parser must skip it and fill all three slots with the next departures
  // (14:37 / 14:44 / 14:52), proving both the past-skip and the 3rd slot.
  static const char *kJson = R"JSON({
    "err": "OK",
    "svcResL": [{
      "res": {
        "common": { "prodL": [
          { "nameS": "S 1" }, { "nameS": "S 2" },
          { "nameS": "S 3" }, { "nameS": "S 4" }
        ] },
        "jnyL": [
          { "date": "20260519", "prodX": 0,
            "stbStop": { "dTimeS": "143200", "dTimeR": "143200" } },
          { "date": "20260519", "prodX": 1,
            "stbStop": { "dTimeS": "143700", "dTimeR": "143700" } },
          { "date": "20260519", "prodX": 2,
            "stbStop": { "dTimeS": "144400", "dTimeR": "144400" } },
          { "date": "20260519", "prodX": 3,
            "stbStop": { "dTimeS": "145200", "dTimeR": "145200" } }
        ]
      }
    }]
  })JSON";
  StreamData out;
  OebbParseResult r;
  // 2026-05-19 14:33:20 CEST — after the 14:32 train, before 14:37.
  const time_t now = 1779194000;
  TEST_ASSERT_TRUE(parseOebbStationBoard(kJson, now, out, r));
  TEST_ASSERT_TRUE(r.filter_matched);
  // 14:32 (1779193920) skipped → slot[0] is the 14:37 S2.
  TEST_ASSERT_TRUE(out.slot[0].valid);
  TEST_ASSERT_TRUE(out.slot[1].valid);
  TEST_ASSERT_TRUE(out.slot[2].valid);
  TEST_ASSERT_EQUAL_STRING("S2", out.slot[0].line_label);
  TEST_ASSERT_EQUAL_STRING("S4", out.slot[2].line_label);
  TEST_ASSERT_EQUAL_INT64(1779194220, out.slot[0].when); // 14:37, not 14:32
  TEST_ASSERT_LESS_THAN(out.slot[1].when, out.slot[0].when);
  TEST_ASSERT_LESS_THAN(out.slot[2].when, out.slot[1].when);
}

void test_parse_malformed_json_returns_false() {
  StreamData out;
  OebbParseResult r;
  TEST_ASSERT_FALSE(parseOebbStationBoard("not json {{", kNoFilter, out, r));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_buildRequest_includes_aid_and_client);
  RUN_TEST(test_buildRequest_includes_stbLoc_dirLoc);
  RUN_TEST(test_buildRequest_includes_products_filter);
  RUN_TEST(test_buildRequest_omits_cfg_block);
  RUN_TEST(test_parse_sample1_two_slots_realtime_S2);
  RUN_TEST(test_parse_sample2_line_labels_S1_S2);
  RUN_TEST(test_parse_sample3_line_labels_S2_S3);
  RUN_TEST(test_parse_sample1_absolute_epoch_via_TZ);
  RUN_TEST(test_parse_cancelled_skipped);
  RUN_TEST(test_parse_plan_only_falls_back_to_Plan_source);
  RUN_TEST(test_parse_err_AID_sets_auth_error_seen);
  RUN_TEST(test_parse_err_AUTH_sets_auth_error_seen);
  RUN_TEST(test_parse_err_FAIL_sets_endpoint_not_responded);
  RUN_TEST(test_parse_empty_jnyL_sets_filter_unmatched);
  RUN_TEST(test_parse_long_line_label_abbreviates_xx);
  RUN_TEST(test_parse_REX_label_strips_to_REX1);
  RUN_TEST(test_parse_skips_departed_journeys);
  RUN_TEST(test_parse_malformed_json_returns_false);
  return UNITY_END();
}
