#include "mock_data.h"

#include <cstring>

namespace bustaferl::mockview {

Departure mkDep(std::time_t when, DepartureSource src, const char *line) {
  Departure d;
  d.when = when;
  d.source = src;
  d.valid = true;
  std::strncpy(d.line_label, line, Departure::LINE_LABEL_CAP - 1);
  d.line_label[Departure::LINE_LABEL_CAP - 1] = '\0';
  return d;
}

StreamSnapshot buildNormalSnapshot() {
  // Offsets and sources match screen-1-normal.png (anchor 18:30 UTC):
  //   58A → Atzg:  18:32 RT / 18:48 PL
  //   58A → Hie:   18:35 RT / 18:50 PL
  //   58B → Atzg:  18:41 RT / 19:01 RT
  //   S-Bahn Hbf:  S2 18:37 RT / S3 18:51 RT  (S7 slot stays empty — §3.3)
  StreamSnapshot s{};
  s.api_ok = true;

  auto &s58a_atz = s.stream[STREAM_58A_ATZ];
  s58a_atz.endpoint_responded = true;
  s58a_atz.filter_matched = true;
  s58a_atz.slot[0] = mkDep(kMockNow + 2 * 60, DepartureSource::Realtime);
  s58a_atz.slot[1] = mkDep(kMockNow + 18 * 60, DepartureSource::Plan);

  auto &s58a_hie = s.stream[STREAM_58A_HIETZING];
  s58a_hie.endpoint_responded = true;
  s58a_hie.filter_matched = true;
  s58a_hie.slot[0] = mkDep(kMockNow + 5 * 60, DepartureSource::Realtime);
  s58a_hie.slot[1] = mkDep(kMockNow + 20 * 60, DepartureSource::Plan);

  auto &s58b_atz = s.stream[STREAM_58B_ATZ];
  s58b_atz.endpoint_responded = true;
  s58b_atz.filter_matched = true;
  s58b_atz.slot[0] = mkDep(kMockNow + 11 * 60, DepartureSource::Realtime);
  s58b_atz.slot[1] = mkDep(kMockNow + 31 * 60, DepartureSource::Realtime);

  auto &sbahn = s.stream[STREAM_SBAHN_HBF];
  sbahn.endpoint_responded = true;
  sbahn.filter_matched = true;
  sbahn.slot[0] = mkDep(kMockNow + 7 * 60, DepartureSource::Realtime, "S2");
  sbahn.slot[1] = mkDep(kMockNow + 21 * 60, DepartureSource::Realtime, "S3");
  sbahn.slot[2] = mkDep(kMockNow + 29 * 60, DepartureSource::Realtime, "S2");

  return s;
}

StreamSnapshot buildNightSnapshot() {
  // Offsets and labels match screen-3-nachtbetrieb.png (anchor 04:00 UTC).
  // All Plan because nightly traffic shows scheduled first departures.
  //   58A → Atzg:  04:23 / 04:38
  //   58A → Hie:   04:31 / 04:46
  //   58B → Atzg:  04:38 / 04:53
  //   S-Bahn Hbf:  S2 04:43 / S3 04:58  (S7 slot stays empty — §3.3)
  StreamSnapshot s{};
  s.api_ok = true;

  auto &s58a_atz = s.stream[STREAM_58A_ATZ];
  s58a_atz.endpoint_responded = true;
  s58a_atz.filter_matched = true;
  s58a_atz.slot[0] = mkDep(kMockNowNight + 23 * 60, DepartureSource::Plan);
  s58a_atz.slot[1] = mkDep(kMockNowNight + 38 * 60, DepartureSource::Plan);

  auto &s58a_hie = s.stream[STREAM_58A_HIETZING];
  s58a_hie.endpoint_responded = true;
  s58a_hie.filter_matched = true;
  s58a_hie.slot[0] = mkDep(kMockNowNight + 31 * 60, DepartureSource::Plan);
  s58a_hie.slot[1] = mkDep(kMockNowNight + 46 * 60, DepartureSource::Plan);

  auto &s58b_atz = s.stream[STREAM_58B_ATZ];
  s58b_atz.endpoint_responded = true;
  s58b_atz.filter_matched = true;
  s58b_atz.slot[0] = mkDep(kMockNowNight + 38 * 60, DepartureSource::Plan);
  s58b_atz.slot[1] = mkDep(kMockNowNight + 53 * 60, DepartureSource::Plan);

  auto &sbahn = s.stream[STREAM_SBAHN_HBF];
  sbahn.endpoint_responded = true;
  sbahn.filter_matched = true;
  sbahn.slot[0] = mkDep(kMockNowNight + 43 * 60, DepartureSource::Plan, "S2");
  sbahn.slot[1] = mkDep(kMockNowNight + 58 * 60, DepartureSource::Plan, "S3");
  sbahn.slot[2] = mkDep(kMockNowNight + 73 * 60, DepartureSource::Plan, "S2");

  return s;
}

} // namespace bustaferl::mockview
