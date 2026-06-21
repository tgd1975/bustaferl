// Host tests for snapshot_fetcher: apiUrlForBatch composition + fetchSnapshot
// OGD batch iteration / merge / failure counting, plus the ÖBB S-Bahn POST
// leg. Uses a FakeNet that routes httpGet by URL substring and answers httpPost
// with a canned HAFAS StationBoard body.

#include "data/oebb_hafas_parse.h"
#include "data/wienerlinien_parse.h"
#include "hal/INetwork.h"
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
  std::vector<std::string> post_urls_seen;
  // url-substring → body to return. First substring match wins.
  std::vector<std::pair<std::string, std::string>> routes;
  bool fail_all = false;
  bool oebb_fail = false;
  std::string oebb_body;

  bool connect(unsigned) override { return true; }
  bool isConnected() override { return true; }
  bool httpGet(const std::string &url, std::string &out) override {
    urls_seen.push_back(url);
    if (fail_all)
      return false;
    for (const auto &r : routes) {
      if (url.find(r.first) != std::string::npos) {
        out = r.second;
        return true;
      }
    }
    return false;
  }
  bool httpPost(const std::string &url, const std::string &, const std::string &,
                std::string &out) override {
    post_urls_seen.push_back(url);
    if (fail_all || oebb_fail)
      return false;
    out = oebb_body;
    return true;
  }
};

// Three OGD bus RBLs (one departure each) — the S-Bahn stream is fed via POST.
const char *kBusBody = R"JSON({
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

// Minimal HAFAS StationBoard with two S-Bahn departures (S2, S3).
const char *kOebbBody = R"JSON({
  "svcResL": [{ "res": {
    "common": { "prodL": [{ "name": "S2" }, { "name": "S3" }] },
    "jnyL": [
      { "stbStop": { "dDateS": "20240101", "dTimeS": "120600" }, "prodX": 0 },
      { "stbStop": { "dDateS": "20240101", "dTimeS": "121200" }, "prodX": 1 }
    ]
  } }],
  "err": "OK"
})JSON";

void buildFilters(StreamFilter (&f)[STREAM_COUNT]) {
  f[STREAM_58A_ATZ] = {1001, "58A", "Atzgersdorf"};
  f[STREAM_58A_HIETZING] = {1002, "58A", "Hietzing"};
  f[STREAM_58B_ATZ] = {1003, "58B", "Atzgersdorf"};
  // STREAM_SBAHN_HBF left default (rbl = 0) — fed by the POST leg.
}

OebbStreamFilter oebbFilter() {
  return OebbStreamFilter{"8100634", "8100002", "63", 6};
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

void test_fetchSnapshot_happy_two_ogd_batches_plus_oebb() {
  // STOPIDS_PER_QUERY=2 → 3 OGD streams split into 2 batches (2+1); plus one
  // ÖBB POST → 3 total batches.
  FakeNet net;
  net.routes.push_back({"stopId=", kBusBody});
  net.oebb_body = kOebbBody;
  StreamFilter f[STREAM_COUNT];
  buildFilters(f);

  StreamSnapshot snap;
  FetchSummary sum;
  bool ok =
      fetchSnapshot(net, "http://x?z=1", "http://mgate", f, oebbFilter(), snap,
                    sum);

  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_TRUE(snap.api_ok);
  TEST_ASSERT_EQUAL_INT(3, sum.total_batches);
  TEST_ASSERT_EQUAL_INT(0, sum.failed_batches);
  TEST_ASSERT_TRUE(sum.oebb_http_ok);
  TEST_ASSERT_EQUAL_INT(2, static_cast<int>(net.urls_seen.size()));
  TEST_ASSERT_EQUAL_INT(1, static_cast<int>(net.post_urls_seen.size()));

  for (int i = 0; i < STREAM_COUNT; ++i) {
    TEST_ASSERT_TRUE_MESSAGE(snap.stream[i].slot[0].valid,
                             "stream slot[0] expected valid");
  }
  // S-Bahn slots carry per-slot line labels from the HAFAS prodL.
  TEST_ASSERT_EQUAL_STRING("S2",
                           snap.stream[STREAM_SBAHN_HBF].slot[0].line_label);
  TEST_ASSERT_EQUAL_STRING("S3",
                           snap.stream[STREAM_SBAHN_HBF].slot[1].line_label);
}

void test_fetchSnapshot_url_uses_fetch_order_first_batch() {
  // FETCH_ORDER = {58B, 58A-Hie, 58A-Atz}; the first paired batch holds 58B
  // (1003) + 58A-Hie (1002), not 58A-Atz (1001, the trailing single query).
  FakeNet net;
  net.routes.push_back({"stopId=", kBusBody});
  net.oebb_body = kOebbBody;
  StreamFilter f[STREAM_COUNT];
  buildFilters(f);

  StreamSnapshot snap;
  FetchSummary sum;
  fetchSnapshot(net, "http://x?z=1", "http://mgate", f, oebbFilter(), snap, sum);

  TEST_ASSERT_TRUE(net.urls_seen.size() >= 1);
  const std::string &first = net.urls_seen[0];
  TEST_ASSERT_NOT_NULL(std::strstr(first.c_str(), "stopId=1003"));
  TEST_ASSERT_NOT_NULL(std::strstr(first.c_str(), "stopId=1002"));
  TEST_ASSERT_NULL(std::strstr(first.c_str(), "stopId=1001"));
}

void test_fetchSnapshot_all_batches_fail_marks_api_not_ok() {
  FakeNet net;
  net.fail_all = true;
  StreamFilter f[STREAM_COUNT];
  buildFilters(f);

  StreamSnapshot snap;
  FetchSummary sum;
  bool ok =
      fetchSnapshot(net, "http://x", "http://mgate", f, oebbFilter(), snap, sum);

  TEST_ASSERT_FALSE(ok);
  TEST_ASSERT_FALSE(snap.api_ok);
  TEST_ASSERT_EQUAL_INT(3, sum.total_batches);
  TEST_ASSERT_EQUAL_INT(3, sum.failed_batches);
  TEST_ASSERT_FALSE(sum.oebb_http_ok);
}

void test_fetchSnapshot_oebb_failure_keeps_ogd_api_ok() {
  // OGD batches succeed, only the ÖBB POST fails → still api_ok, one failed
  // batch, S-Bahn stream empty.
  FakeNet net;
  net.routes.push_back({"stopId=", kBusBody});
  net.oebb_fail = true;
  StreamFilter f[STREAM_COUNT];
  buildFilters(f);

  StreamSnapshot snap;
  FetchSummary sum;
  bool ok =
      fetchSnapshot(net, "http://x?z=1", "http://mgate", f, oebbFilter(), snap,
                    sum);

  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_TRUE(snap.api_ok);
  TEST_ASSERT_EQUAL_INT(3, sum.total_batches);
  TEST_ASSERT_EQUAL_INT(1, sum.failed_batches);
  TEST_ASSERT_FALSE(sum.oebb_http_ok);
  TEST_ASSERT_FALSE(snap.stream[STREAM_SBAHN_HBF].slot[0].valid);
  TEST_ASSERT_TRUE(snap.stream[STREAM_58A_ATZ].slot[0].valid);
}

void test_fetchSnapshot_ogd_partial_failure_keeps_api_ok() {
  // One OGD batch fails to parse (malformed body for the 58B/58A-Hie pair);
  // the other OGD batch + ÖBB succeed.
  FakeNet net;
  net.routes.push_back({"stopId=1003", "{ this is not json"});
  net.routes.push_back({"stopId=", kBusBody});
  net.oebb_body = kOebbBody;
  StreamFilter f[STREAM_COUNT];
  buildFilters(f);

  StreamSnapshot snap;
  FetchSummary sum;
  bool ok =
      fetchSnapshot(net, "http://x?z=1", "http://mgate", f, oebbFilter(), snap,
                    sum);

  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_TRUE(snap.api_ok);
  TEST_ASSERT_EQUAL_INT(3, sum.total_batches);
  TEST_ASSERT_EQUAL_INT(1, sum.failed_batches);
  TEST_ASSERT_FALSE(snap.stream[STREAM_58B_ATZ].slot[0].valid);
  TEST_ASSERT_FALSE(snap.stream[STREAM_58A_HIETZING].slot[0].valid);
  TEST_ASSERT_TRUE(snap.stream[STREAM_58A_ATZ].slot[0].valid);
  TEST_ASSERT_TRUE(snap.stream[STREAM_SBAHN_HBF].slot[0].valid);
}

void test_fetchSnapshot_reset_clears_previous_out_state() {
  FakeNet net;
  net.fail_all = true;

  StreamFilter f[STREAM_COUNT];
  buildFilters(f);

  StreamSnapshot snap;
  snap.api_ok = true;
  snap.stream[STREAM_58A_ATZ].slot[0].valid = true;
  snap.stream[STREAM_58A_ATZ].slot[0].when = 999;
  FetchSummary sum;
  sum.total_batches = 99;
  sum.failed_batches = 7;

  fetchSnapshot(net, "http://x", "http://mgate", f, oebbFilter(), snap, sum);

  TEST_ASSERT_FALSE(snap.api_ok);
  TEST_ASSERT_FALSE(snap.stream[STREAM_58A_ATZ].slot[0].valid);
  TEST_ASSERT_EQUAL_INT(3, sum.total_batches);
  TEST_ASSERT_EQUAL_INT(3, sum.failed_batches);
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
  RUN_TEST(test_fetchSnapshot_happy_two_ogd_batches_plus_oebb);
  RUN_TEST(test_fetchSnapshot_url_uses_fetch_order_first_batch);
  RUN_TEST(test_fetchSnapshot_all_batches_fail_marks_api_not_ok);
  RUN_TEST(test_fetchSnapshot_oebb_failure_keeps_ogd_api_ok);
  RUN_TEST(test_fetchSnapshot_ogd_partial_failure_keeps_api_ok);
  RUN_TEST(test_fetchSnapshot_reset_clears_previous_out_state);
  return UNITY_END();
}
