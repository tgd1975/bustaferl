#ifndef BUSTAFERL_RENDER_DISPLAY_STATE_H
#define BUSTAFERL_RENDER_DISPLAY_STATE_H

#include "render/canvas.h"

#include <ctime>

namespace bustaferl {

void drawBoot(render::Canvas &canvas, const char *version_str);
void drawOffline(render::Canvas &canvas, std::time_t last_fetch_at,
                 int retry_in_s);
void drawAuth(render::Canvas &canvas, const char *aid_short, int http_code);
void drawQuiet(render::Canvas &canvas);

} // namespace bustaferl

#endif
