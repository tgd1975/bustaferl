#ifndef BUSTAFERL_MOCKVIEW_MOCK_DATA_H
#define BUSTAFERL_MOCKVIEW_MOCK_DATA_H

// Portable mock-data builders. No NATIVE_BUILD guard so the host-side
// PNG-dump test (test_native_mockview_dump) can render the same StreamSnapshot
// that the on-device mockview-N firmware flashes — keeps docs/screenshots/host
// in lockstep with docs/screenshots/device.

#include "data/Departure.h"
#include "data/StreamSnapshot.h"

#include <ctime>

namespace bustaferl::mockview {

// Anchor times for mock departures. The mockview firmware never calls
// Esp32Clock::setEnvTz, so localtime_r() effectively renders UTC — UTC
// anchors are picked so the displayed HH:MM is reproducible without a TZ
// shim. kMockNowNight anchors the overnight, schedule-only board.
constexpr std::time_t kMockNow = 1779647400;      // 2026-05-24 18:30:00 UTC
constexpr std::time_t kMockNowNight = 1779681600; // 2026-05-25 04:00:00 UTC

// Convenience constructor for Departure — bypasses aggregate-init pitfalls
// caused by the LINE_LABEL_CAP char array.
Departure mkDep(std::time_t when, DepartureSource src, const char *line = "");

// 4-stream snapshot matching screen-1-normal.png (anchor 18:30 UTC):
//   58A → Atzgersdorf:  realtime +2 / plan     +18  (18:32 / 18:48□)
//   58A → Hietzing:     realtime +5 / plan     +20  (18:35 / 18:50□)
//   58B → Atzgersdorf:  realtime +11 / realtime +31 (18:41 / 19:01)
//   S-Bahn → Hbf:       realtime +7 S2 / realtime +21 S3 (S7 slot empty per §3.3)
// The everyday mixed (live + scheduled) board.
StreamSnapshot buildNormalSnapshot();

// Overnight board: all scheduled (Plan) departures, no realtime (anchor 04:00
// UTC). Drives the schedule-only demo screen.
StreamSnapshot buildScheduleOnlySnapshot();

} // namespace bustaferl::mockview

#endif
