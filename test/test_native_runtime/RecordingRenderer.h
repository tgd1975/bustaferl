#ifndef BUSTAFERL_NATIVE_RUNTIME_RECORDINGRENDERER_H
#define BUSTAFERL_NATIVE_RUNTIME_RECORDINGRENDERER_H

#include "../../src/hal/IRenderer.h"
#include "../../src/render/layout.h"

#include <string>

namespace bustaferl::native_runtime {

// Headless renderer: produces a deterministic 1-bit pseudo-raster derived
// from the StreamSnapshot's departure timestamps, then dumps the framebuffer
// as a P5 PGM whenever it differs from the previous render. The dump count
// equals the count of effective display updates — a leak in the render path
// shows up as PGM-count growing faster than expected.
//
// NOT a layout test: the raster pattern is a hash, not the production GFX
// output (Adafruit-GFX is firmware-only). Real layout regressions stay in
// test_device_render.
class RecordingRenderer : public IRenderer {
public:
  explicit RecordingRenderer(std::string dump_dir);

  void render(const RenderInput &in, Frame &fb) override;

  // Diagnostics — exposed so the smoke test can assert on them.
  unsigned dump_count() const { return dump_count_; }
  unsigned render_count() const { return render_count_; }

private:
  void renderDeterministic(const RenderInput &in, Frame &fb);
  bool framesEqual(const Frame &a, const Frame &b) const;
  void writePgm(const Frame &fb, const std::string &path) const;

  std::string dump_dir_;
  Frame prev_;
  bool have_prev_ = false;
  unsigned dump_count_ = 0;
  unsigned render_count_ = 0;
};

} // namespace bustaferl::native_runtime

#endif
