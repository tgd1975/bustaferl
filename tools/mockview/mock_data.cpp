#ifndef NATIVE_BUILD

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
  StreamSnapshot s{};
  s.api_ok = true;

  auto &s58a_atz = s.stream[STREAM_58A_ATZ];
  s58a_atz.endpoint_responded = true;
  s58a_atz.filter_matched = true;
  s58a_atz.slot[0] = mkDep(kMockNow + 4 * 60, DepartureSource::Realtime);
  s58a_atz.slot[1] = mkDep(kMockNow + 14 * 60, DepartureSource::Realtime);

  auto &s58a_hie = s.stream[STREAM_58A_HIETZING];
  s58a_hie.endpoint_responded = true;
  s58a_hie.filter_matched = true;
  s58a_hie.slot[0] = mkDep(kMockNow + 7 * 60, DepartureSource::Realtime);
  s58a_hie.slot[1] = mkDep(kMockNow + 22 * 60, DepartureSource::Plan);

  auto &s58b_atz = s.stream[STREAM_58B_ATZ];
  s58b_atz.endpoint_responded = true;
  s58b_atz.filter_matched = true;
  s58b_atz.slot[0] = mkDep(kMockNow + 11 * 60, DepartureSource::Realtime);

  auto &sbahn = s.stream[STREAM_SBAHN_HBF];
  sbahn.endpoint_responded = true;
  sbahn.filter_matched = true;
  sbahn.slot[0] = mkDep(kMockNow + 6 * 60, DepartureSource::Realtime, "S2");
  sbahn.slot[1] = mkDep(kMockNow + 21 * 60, DepartureSource::Plan, "S2");

  return s;
}

StreamSnapshot buildNightSnapshot() {
  StreamSnapshot s{};
  s.api_ok = true;

  auto &s58a_atz = s.stream[STREAM_58A_ATZ];
  s58a_atz.endpoint_responded = true;
  s58a_atz.filter_matched = true;
  s58a_atz.slot[0] = mkDep(kMockNow + (20 * 3600 + 42 * 60), DepartureSource::Plan);
  s58a_atz.slot[1] = mkDep(kMockNow + (20 * 3600 + 57 * 60), DepartureSource::Plan);

  auto &s58a_hie = s.stream[STREAM_58A_HIETZING];
  s58a_hie.endpoint_responded = true;
  s58a_hie.filter_matched = true;
  s58a_hie.slot[0] = mkDep(kMockNow + (20 * 3600 + 48 * 60), DepartureSource::Plan);
  s58a_hie.slot[1] = mkDep(kMockNow + (21 * 3600 + 3 * 60), DepartureSource::Plan);

  auto &s58b_atz = s.stream[STREAM_58B_ATZ];
  s58b_atz.endpoint_responded = true;
  s58b_atz.filter_matched = true;
  s58b_atz.slot[0] = mkDep(kMockNow + (20 * 3600 + 55 * 60), DepartureSource::Plan);

  auto &sbahn = s.stream[STREAM_SBAHN_HBF];
  sbahn.endpoint_responded = true;
  sbahn.filter_matched = true;
  sbahn.slot[0] = mkDep(kMockNow + (20 * 3600 + 38 * 60), DepartureSource::Plan, "S2");
  sbahn.slot[1] = mkDep(kMockNow + (20 * 3600 + 53 * 60), DepartureSource::Plan, "S2");

  return s;
}

} // namespace bustaferl::mockview

#endif
