#ifndef BUSTAFERL_LOGIC_DIAG_MODE_H
#define BUSTAFERL_LOGIC_DIAG_MODE_H

#include "button_classifier.h" // ButtonPress

namespace bustaferl {

// Diagnostic pager (entered by a double-click in normal operation). Four
// plain-text pages the user flips through to understand an anomaly the panel
// showed — the device keeps no serial log, so this is the only window into
// "what happened". Forward-only with wrap; a long press leaves the mode.
enum class DiagPage : std::uint8_t {
  Status = 0,     // WLAN / clock / per-stream self-test / heap / uptime
  Cycles = 1,     // recent cycle history (newest first)
  Errors = 2,     // recent anomalies (newest first)
  DataDetail = 3, // per-slot source + age + schedule hints + panel state
};

constexpr int DIAG_PAGE_COUNT = 4;

enum class DiagAction : std::uint8_t {
  ShowPage, // stay in the mode, render `page`
  Exit,     // leave diagnostic mode, resume normal operation
};

struct DiagStep {
  DiagAction action = DiagAction::ShowPage;
  int page = 0;
};

// Decide the next step given the current page and the interaction that ended
// the wait. `press` is the classified verdict (Short = advance one page with
// wrap, Long = exit; None leaves the page unchanged). `timed_out` forces Exit
// regardless of `press` — the safety net so a forgotten session does not keep
// the device out of its normal poll loop forever. Pure; host-tested.
DiagStep diagNext(int current_page, ButtonPress press, bool timed_out);

} // namespace bustaferl

#endif
