#ifndef BUSTAFERL_MOCKVIEW_MOCK_DATA_H
#define BUSTAFERL_MOCKVIEW_MOCK_DATA_H

#ifndef NATIVE_BUILD

#include "data/Departure.h"
#include "data/StreamSnapshot.h"

#include <ctime>

namespace bustaferl::mockview {

// Anchor time for all mock departures: 2026-05-19 08:30:00 Europe/Vienna.
// Fixed so renders are reproducible across flashes.
constexpr std::time_t kMockNow = 1779597000;

// Convenience constructor for Departure — bypasses aggregate-init pitfalls
// caused by the LINE_LABEL_CAP char array.
Departure mkDep(std::time_t when, DepartureSource src, const char *line = "");

// Realistic 4-stream snapshot:
//   58A → Atzgersdorf:  realtime now+4 / realtime now+14
//   58A → Hietzing:     realtime now+7 / plan     now+22
//   58B → Atzgersdorf:  realtime now+11 / (empty)
//   S-Bahn → Hbf:       realtime now+6 S2 / plan now+21 S2
// Used by the Normal and Stale firmwares.
StreamSnapshot buildNormalSnapshot();

// Night snapshot: morning first-departures only, all plan-fallback.
// Times anchored ~20h after kMockNow to land in early morning.
StreamSnapshot buildNightSnapshot();

} // namespace bustaferl::mockview

#endif
#endif
