#ifndef BUSTAFERL_ERROR_OVERLAY_H
#define BUSTAFERL_ERROR_OVERLAY_H

#include "layout.h"

namespace bustaferl {

// Convenience: produce a stale-overlay frame. Wipes departure slots and
// stamps the "VERALTET" banner. Useful when the API has been silent past the
// stale threshold.
void renderStaleFrame(Frame &fb);

// "Start fehlgeschlagen" full-screen splash for cold-boot give-up state.
void renderStartFailedFrame(Frame &fb);

} // namespace bustaferl

#endif
