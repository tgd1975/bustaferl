#ifndef BUSTAFERL_IRENDERER_H
#define BUSTAFERL_IRENDERER_H

#include "../render/layout.h"

namespace bustaferl {

class IRenderer {
public:
  virtual ~IRenderer() = default;
  virtual void render(const RenderInput &in, Frame &fb) = 0;
};

} // namespace bustaferl

#endif
