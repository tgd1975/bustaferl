#ifndef BUSTAFERL_MOCKVIEW_RUNNER_H
#define BUSTAFERL_MOCKVIEW_RUNNER_H

#ifndef NATIVE_BUILD

#include "render/layout.h"

namespace bustaferl::mockview {

// Boots the e-paper, renders one RenderInput into a heap-allocated Frame,
// pushes the frame with a full refresh, then deep-sleeps without a wake
// source. Used by the seven Session E mock-view firmwares (one per state).
void runMockView(const RenderInput &in);

} // namespace bustaferl::mockview

#endif
#endif
