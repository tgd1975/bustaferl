#ifndef BUSTAFERL_RENDER_DISPLAY_STATE_H
#define BUSTAFERL_RENDER_DISPLAY_STATE_H

#include "hal/ScanResult.h"
#include "render/canvas.h"

#include <ctime>

namespace bustaferl {

void drawBoot(render::Canvas &canvas, const char *version_str);
// Inverted (ink background, paper text) one-shot overlay shown after an
// unplanned reset. `reason_text` is the human-readable cause, e.g.
// "BROWNOUT" or "WATCHDOG/PANIC".
void drawBrownoutScreen(render::Canvas &canvas, const char *reason_text);
void drawOffline(render::Canvas &canvas, std::time_t last_fetch_at,
                 int retry_in_s, const ScanResult &visible_aps,
                 const ConfiguredSsids &wanted_ssids,
                 const SsidCaseMismatch &case_mismatch);
void drawAuth(render::Canvas &canvas, const char *aid_short, int http_code);
void drawWifiAuth(render::Canvas &canvas, const ConfiguredSsids &wanted_ssids);

} // namespace bustaferl

#endif
