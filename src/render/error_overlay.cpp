#include "error_overlay.h"

#ifndef NATIVE_BUILD

namespace bustaferl {

void renderStaleFrame(Frame& fb) {
    RenderInput in;
    in.overlay = OverlayKind::Stale;
    renderFrame(in, fb);
}

void renderStartFailedFrame(Frame& fb) {
    RenderInput in;
    in.overlay = OverlayKind::StartFailed;
    renderFrame(in, fb);
}

}  // namespace bustaferl

#endif
