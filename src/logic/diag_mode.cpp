#include "logic/diag_mode.h"

namespace bustaferl {

DiagStep diagNext(int current_page, ButtonPress press, bool timed_out) {
  if (timed_out || press == ButtonPress::Long) {
    return DiagStep{DiagAction::Exit, current_page};
  }
  // Short (and a double-click landing inside the mode) advances one page,
  // wrapping after the last. None leaves the page as-is.
  if (press == ButtonPress::Short || press == ButtonPress::Double) {
    int next = current_page + 1;
    if (next >= DIAG_PAGE_COUNT) {
      next = 0;
    }
    return DiagStep{DiagAction::ShowPage, next};
  }
  return DiagStep{DiagAction::ShowPage, current_page};
}

} // namespace bustaferl
