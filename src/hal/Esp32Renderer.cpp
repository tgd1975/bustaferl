#ifndef NATIVE_BUILD

#include "Esp32Renderer.h"

#include <esp_heap_caps.h>
#include <esp_system.h>

namespace bustaferl {

void Esp32Renderer::render(const RenderInput &in, Frame &fb) {
  if (in.overlay == OverlayKind::Boot && in.boot_report.valid) {
    // The platform-neutral cycle cannot probe the heap — patch the live
    // values in here, right before the dashboard is drawn.
    RenderInput patched = in;
    patched.boot_report.heap_free_bytes = esp_get_free_heap_size();
    patched.boot_report.heap_largest_bytes =
        heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);
    renderFrame(patched, fb);
    return;
  }
  renderFrame(in, fb);
}

} // namespace bustaferl

#endif
