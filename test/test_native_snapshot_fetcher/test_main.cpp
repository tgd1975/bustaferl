// Host tests for snapshot_fetcher: apiUrlForBatch composition + v2
// fetchSnapshot (OGD batch loop + HAFAS POST + PersistedMeta tripwire).
// Uses a FakeNet that routes by URL substring (mirrors test_native_
// schedule_fetcher).

#include "data/oebb_hafas_parse.h"
#include "data/wienerlinien_parse.h"
#include "hal/INetwork.h"
#include "hal/IPersistentStore.h"
#include "logic/snapshot_fetcher.h"

#include <cstring>
#include <string>
#include <unity.h>
#include <vector>

using namespace bustaferl;

namespace {

class FakeNet : public INetwork {
public:
  std::vector<std::string> urls_seen;
  std::vector<std::string> posts_seen; // bodies of POST calls
  // url-substring → body to return. First substring match wins.
  std::vector<std::pair<std::string, std::string>> get_routes;
  std::vector<std::pair<std::string, std::string>> post_routes;
  // optional per-route HTTP status; default 200 on hit, 0 on miss.
  std::vector<std::pair<std::string, int>> get_status_overrides;
  std::vector<std::pair<std::string, int>> post_status_overrides;
  bool fail_all_gets = false;

  bool connect(unsigned) override { return true; }
  bool isConnected() override { return true; }

  HttpResult httpGet(const std::string &url, std::string &out) override {
    urls_seen.push_back(url);
    if (fail_all_gets)
      return {false, 0};
    int status = 200;
    for (const auto &s : get_status_overrides) {
      if (url.find(s.first) != std::string::npos) {
        status = s.second;
        break;
      }
    }
    for (const auto &r : get_routes) {
      if (url.find(r.first) != std::string::npos) {
        out = r.second;
        return {true, status};
      }
    }
    return {status != 0, status == 200 ? 0 : status};
  }

  HttpResult httpPost(const std::string &url, const std::string &body,
                      const std::string &, std::string &out) override {
    urls_seen.push_back(url);
    posts_seen.push_back(body);
    int status = 200;
    for (const auto &s : post_status_overrides) {
      if (url.find(s.first) != std::string::npos) {
        status = s.second;
        break;
      }
    }
    for (const auto &r : post_routes) {
      if (url.find(r.first) != std::string::npos) {
        out = r.second;
        return {true, status};
      }
    }
    return {false, 0};
  }
};

// OGD body with three bus RBLs matching the v2 filter table. Each RBL has
// exactly one departure.
const char *kOgdBody = R"JSON({
  "data": {
    "monitors": [
      {
        "locationStop": { "properties": { "attributes": { "rbl": 1001 } } },
        "lines": [{ "name": "58A", "towards": "Atzgersdorf",
          "departures": { "departure": [
            { "departureTime": { "timePlanned": "2024-01-01T12:01:00.000+0100" } }
          ] } }]
      },
      {
        "locationStop": { "properties": { "attributes": { "rbl": 1002 } } },
        "lines": [{ "name": "58A", "towards": "Hietzing",
          "departures": { "departure": [
            { "departureTime": { "timePlanned": "2024-01-01T12:02:00.000+0100" } }
          ] } }]
      },
      {
        "locationStop": { "properties": { "attributes": { "rbl": 1003 } } },
        "lines": [{ "name": "58B", "towards": "Atzgersdorf",
          "departures": { "departure": [
            { "departureTime": { "timePlanned": "2024-01-01T12:03:00.000+0100" } }
          ] } }]
      }
    ]
  }
})JSON";

// HAFAS body — minimal err=OK with one jny and one common.prodL entry. The
// Parser only needs jny.date / jny.prodX / jny.stbStop.dTime{S,R}.
const char *kHafasOk = R"JSON({
  "err": "OK",
  "svcResL": [{
    "res": {
      "common": { "prodL": [ { "nameS": "S 2" } ] },
      "jnyL": [{
        "date": "20260519",
        "prodX": 0,
        "stbStop": { "dTimeS": "143200", "dTimeR": "143200" }
      }]
    }
  }]
})JSON";

const char *kHafasAidErr = R"JSON({ "err": "AID" })JSON";

void buildFilters(StreamFilter (&f)[STREAM_COUNT]) {
  f[STREAM_58A_ATZ] = {1001, "58A", "Atzgersdorf"};
  f[STREAM_58A_HIETZING] = {1002, "58A", "Hietzing"};
  f[STREAM_58B_ATZ] = {1003, "58B", "Atzgersdorf"};
  // STREAM_SBAHN_HBF intentionally default — rbl=0.
}

OebbStreamFilter makeOebbFilter() {
  OebbStreamFilter f;
  f.stbloc_extid = "1292301";
  f.dirloc_extid = "1290401";
  f.products = "63";
  f.max_jny = 6;
  return f;
}

} // namespace

void test_apiUrlForBatch_appends_stopIds() {
  int ids[3] = {1001, 1002, 1003};
  std::string url = apiUrlForBatch("http://x?z=1", ids, 3);
  TEST_ASSERT_EQUAL_STRING("http://x?z=1&stopId=1001&stopId=1002&stopId=1003",
                           url.c_str());
}

void test_apiUrlForBatch_empty_count_returns_base() {
  int ids[1] = {0};
  std::string url = apiUrlForBatch("http://x", ids, 0);
  TEST_ASSERT_EQUAL_STRING("http://x", url.c_str());
}

void test_fetchSnapshot_two_endpoints_populate_all_four_streams() {
  // OGD-batch returns kOgdBody for every stopId query, HAFAS-POST returns
  // kHafasOk for fahrplan.oebb.at. After fetchSnapshot: 3 bus streams +
  // 1 S-Bahn stream populated; summary counts 2 OGD batches + 1 OEBB call.
  FakeNet net;
  net.get_routes.push_back({"stopId=", kOgdBody});
  net.post_routes.push_back({"mgate", kHafasOk});

  StreamFilter f[STREAM_COUNT];
  buildFilters(f);
  OebbStreamFilter oef = makeOebbFilter();

  StreamSnapshot snap;
  FetchSummary sum;
  PersistedMeta meta;
  std::string ogd = "http://ogd?z=1";
  std::string mg = "http://mgate";
  bool ok = fetchSnapshot(net, FetchInputs{ogd, mg, f, oef}, snap, sum, meta);

  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_TRUE(snap.api_ok);
  // 3 OGD streams ÷ 2 stopIds/batch = 2 batches, plus 1 HAFAS call = 3.
  TEST_ASSERT_EQUAL_INT(3, sum.total_batches);
  TEST_ASSERT_EQUAL_INT(0, sum.failed_batches);

  TEST_ASSERT_TRUE(snap.stream[STREAM_58A_ATZ].slot[0].valid);
  TEST_ASSERT_TRUE(snap.stream[STREAM_58A_HIETZING].slot[0].valid);
  TEST_ASSERT_TRUE(snap.stream[STREAM_58B_ATZ].slot[0].valid);
  TEST_ASSERT_TRUE(snap.stream[STREAM_SBAHN_HBF].slot[0].valid);
  TEST_ASSERT_EQUAL_STRING("S2",
                           snap.stream[STREAM_SBAHN_HBF].slot[0].line_label);
  TEST_ASSERT_TRUE(snap.stream[STREAM_SBAHN_HBF].endpoint_responded);
  TEST_ASSERT_TRUE(snap.stream[STREAM_SBAHN_HBF].filter_matched);
}

void test_fetchSnapshot_hafas_post_body_carries_filter() {
  FakeNet net;
  net.get_routes.push_back({"stopId=", kOgdBody});
  net.post_routes.push_back({"mgate", kHafasOk});

  StreamFilter f[STREAM_COUNT];
  buildFilters(f);

  StreamSnapshot snap;
  FetchSummary sum;
  PersistedMeta meta;
  std::string ogd = "http://ogd?z=1";
  std::string mg = "http://mgate";
  OebbStreamFilter oef = makeOebbFilter();
  fetchSnapshot(net, FetchInputs{ogd, mg, f, oef}, snap, sum, meta);

  TEST_ASSERT_EQUAL_INT(1, static_cast<int>(net.posts_seen.size()));
  const std::string &body = net.posts_seen[0];
  TEST_ASSERT_NOT_NULL(std::strstr(body.c_str(), "StationBoard"));
  TEST_ASSERT_NOT_NULL(std::strstr(body.c_str(), "1292301"));
}

void test_fetchSnapshot_hafas_aid_error_sets_auth_flag() {
  FakeNet net;
  net.get_routes.push_back({"stopId=", kOgdBody});
  net.post_routes.push_back({"mgate", kHafasAidErr});

  StreamFilter f[STREAM_COUNT];
  buildFilters(f);

  StreamSnapshot snap;
  FetchSummary sum;
  PersistedMeta meta;
  std::string ogd = "http://ogd?z=1";
  std::string mg = "http://mgate";
  OebbStreamFilter oef = makeOebbFilter();
  fetchSnapshot(net, FetchInputs{ogd, mg, f, oef}, snap, sum, meta);

  TEST_ASSERT_TRUE_MESSAGE(meta.auth_error_seen,
                           "HAFAS err=AID must flip auth_error_seen");
  TEST_ASSERT_FALSE(snap.stream[STREAM_SBAHN_HBF].endpoint_responded);
}

void test_fetchSnapshot_hafas_ok_clears_auth_flag() {
  FakeNet net;
  net.get_routes.push_back({"stopId=", kOgdBody});
  net.post_routes.push_back({"mgate", kHafasOk});

  StreamFilter f[STREAM_COUNT];
  buildFilters(f);

  StreamSnapshot snap;
  FetchSummary sum;
  PersistedMeta meta;
  meta.auth_error_seen = true; // simulate prior tripwire
  std::string ogd = "http://ogd?z=1";
  std::string mg = "http://mgate";
  OebbStreamFilter oef = makeOebbFilter();
  fetchSnapshot(net, FetchInputs{ogd, mg, f, oef}, snap, sum, meta);

  TEST_ASSERT_FALSE_MESSAGE(meta.auth_error_seen,
                            "fresh HAFAS err=OK must clear auth_error_seen");
}

void test_fetchSnapshot_ogd_401_streak_promotes_to_auth_flag() {
  // Every OGD call returns 401 → after the second batch, streak reaches the
  // tripwire (OGD_AUTH_STREAK_TRIPWIRE=3) only if a single test loop hits it.
  // We get 2 batches per fetchSnapshot, so we need two fetch calls to reach
  // 3 increments — but the third still falls within the second fetch's
  // second batch. The point of this test: a single fetchSnapshot already
  // increments the streak, and after enough calls it flips the flag.
  FakeNet net;
  net.get_status_overrides.push_back({"stopId=", 401});
  // No routes → no body, but status comes through as 401.
  StreamFilter f[STREAM_COUNT];
  buildFilters(f);

  StreamSnapshot snap;
  FetchSummary sum;
  PersistedMeta meta;
  std::string ogd = "http://ogd?z=1";
  std::string mg_empty;
  OebbStreamFilter oef = makeOebbFilter();
  // First call: 2 batches × 401 → streak = 2, flag still false.
  fetchSnapshot(net, FetchInputs{ogd, mg_empty, f, oef}, snap, sum, meta);
  TEST_ASSERT_EQUAL_INT(2, meta.ogd_auth_streak);
  TEST_ASSERT_FALSE(meta.auth_error_seen);

  // Second call: first batch flips streak to 3, sets flag.
  fetchSnapshot(net, FetchInputs{ogd, mg_empty, f, oef}, snap, sum, meta);
  TEST_ASSERT_TRUE(meta.auth_error_seen);
  TEST_ASSERT_GREATER_OR_EQUAL_INT(OGD_AUTH_STREAK_TRIPWIRE,
                                   meta.ogd_auth_streak);
}

void test_fetchSnapshot_ogd_2xx_resets_streak() {
  FakeNet net;
  net.get_routes.push_back({"stopId=", kOgdBody});
  StreamFilter f[STREAM_COUNT];
  buildFilters(f);

  StreamSnapshot snap;
  FetchSummary sum;
  PersistedMeta meta;
  meta.ogd_auth_streak = 2;
  std::string ogd = "http://ogd?z=1";
  std::string mg_empty;
  OebbStreamFilter oef = makeOebbFilter();
  fetchSnapshot(net, FetchInputs{ogd, mg_empty, f, oef}, snap, sum, meta);
  TEST_ASSERT_EQUAL_INT(0, meta.ogd_auth_streak);
}

void test_fetchSnapshot_empty_mgate_url_skips_hafas_call() {
  FakeNet net;
  net.get_routes.push_back({"stopId=", kOgdBody});

  StreamFilter f[STREAM_COUNT];
  buildFilters(f);

  StreamSnapshot snap;
  FetchSummary sum;
  PersistedMeta meta;
  std::string ogd = "http://ogd?z=1";
  std::string mg_empty;
  OebbStreamFilter oef = makeOebbFilter();
  fetchSnapshot(net, FetchInputs{ogd, mg_empty, f, oef}, snap, sum, meta);

  TEST_ASSERT_EQUAL_INT(0, static_cast<int>(net.posts_seen.size()));
  // 2 OGD batches only.
  TEST_ASSERT_EQUAL_INT(2, sum.total_batches);
}

void test_fetchSnapshot_all_ogd_fail_keeps_hafas_data() {
  // OGD-fails (transport error) — HAFAS succeeds. api_ok should still be
  // true because at least one batch produced valid JSON.
  FakeNet net;
  net.fail_all_gets = true;
  net.post_routes.push_back({"mgate", kHafasOk});

  StreamFilter f[STREAM_COUNT];
  buildFilters(f);

  StreamSnapshot snap;
  FetchSummary sum;
  PersistedMeta meta;
  std::string ogd = "http://ogd";
  std::string mg = "http://mgate";
  OebbStreamFilter oef = makeOebbFilter();
  bool ok = fetchSnapshot(net, FetchInputs{ogd, mg, f, oef}, snap, sum, meta);
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_TRUE(snap.stream[STREAM_SBAHN_HBF].slot[0].valid);
  TEST_ASSERT_FALSE(snap.stream[STREAM_58A_ATZ].slot[0].valid);
  TEST_ASSERT_EQUAL_INT(3, sum.total_batches);
  TEST_ASSERT_EQUAL_INT(2, sum.failed_batches);
}

void test_fetchSnapshot_reset_clears_previous_out_state() {
  FakeNet net;
  net.fail_all_gets = true;

  StreamFilter f[STREAM_COUNT];
  buildFilters(f);

  StreamSnapshot snap;
  snap.api_ok = true;
  snap.stream[STREAM_58A_ATZ].slot[0].valid = true;
  snap.stream[STREAM_58A_ATZ].slot[0].when = 999;
  FetchSummary sum;
  sum.total_batches = 99;
  sum.failed_batches = 7;
  PersistedMeta meta;

  std::string ogd = "http://x";
  std::string mg_empty;
  OebbStreamFilter oef = makeOebbFilter();
  fetchSnapshot(net, FetchInputs{ogd, mg_empty, f, oef}, snap, sum, meta);

  TEST_ASSERT_FALSE(snap.api_ok);
  TEST_ASSERT_FALSE(snap.stream[STREAM_58A_ATZ].slot[0].valid);
  TEST_ASSERT_EQUAL_INT(2, sum.total_batches); // mgate skipped
  TEST_ASSERT_EQUAL_INT(2, sum.failed_batches);
}

void setUp() {
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();
}
void tearDown() {}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_apiUrlForBatch_appends_stopIds);
  RUN_TEST(test_apiUrlForBatch_empty_count_returns_base);
  RUN_TEST(test_fetchSnapshot_two_endpoints_populate_all_four_streams);
  RUN_TEST(test_fetchSnapshot_hafas_post_body_carries_filter);
  RUN_TEST(test_fetchSnapshot_hafas_aid_error_sets_auth_flag);
  RUN_TEST(test_fetchSnapshot_hafas_ok_clears_auth_flag);
  RUN_TEST(test_fetchSnapshot_ogd_401_streak_promotes_to_auth_flag);
  RUN_TEST(test_fetchSnapshot_ogd_2xx_resets_streak);
  RUN_TEST(test_fetchSnapshot_empty_mgate_url_skips_hafas_call);
  RUN_TEST(test_fetchSnapshot_all_ogd_fail_keeps_hafas_data);
  RUN_TEST(test_fetchSnapshot_reset_clears_previous_out_state);
  return UNITY_END();
}
