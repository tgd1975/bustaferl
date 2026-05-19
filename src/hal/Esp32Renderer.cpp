#ifndef NATIVE_BUILD

#include "Esp32Renderer.h"

namespace bustaferl {

void Esp32Renderer::render(const RenderInput &in, Frame &fb) {
  renderFrame(in, fb);
}

} // namespace bustaferl

#endif
