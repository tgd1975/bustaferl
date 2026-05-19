#ifndef BUSTAFERL_ESP32RENDERER_H
#define BUSTAFERL_ESP32RENDERER_H

#ifndef NATIVE_BUILD

#include "IRenderer.h"

namespace bustaferl {

class Esp32Renderer : public IRenderer {
public:
  void render(const RenderInput &in, Frame &fb) override;
};

} // namespace bustaferl

#endif
#endif
