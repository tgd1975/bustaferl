#include "display_apply.h"

namespace bustaferl {

void applyDisplayDecision(IDisplay &display, const RefreshDecision &d,
                          const uint8_t *fb, PersistedMeta &meta, time_t now) {
  switch (d.kind) {
  case RefreshKind::None:
    break;
  case RefreshKind::Partial:
    display.drawPartial(fb, d.bbox);
    ++meta.partial_count;
    break;
  case RefreshKind::LightFull:
    display.lightFull(fb);
    meta.last_light_full = now;
    meta.partial_count = 0;
    break;
  case RefreshKind::DeepClean:
    display.deepClean(fb);
    meta.last_deep_clean = now;
    meta.last_light_full = now;
    meta.partial_count = 0;
    break;
  }
}

} // namespace bustaferl
