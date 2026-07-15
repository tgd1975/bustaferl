#ifndef BUSTAFERL_NATIVE_RUNTIME_LOGGINGADAPTERS_H
#define BUSTAFERL_NATIVE_RUNTIME_LOGGINGADAPTERS_H

#include "../../src/hal/IDisplay.h"
#include "../../src/hal/IRenderer.h"
#include "../../src/hal/ISleep.h"
#include "RunLog.h"

#include <cstdio>
#include <ctime>

namespace bustaferl::native_runtime {

// Decorators that trace every HAL interaction into the RunLog while
// forwarding to the real adapter. Together they answer the soak's two
// standing questions from the log alone:
//   - "what did the data layer deliver, what did the panel show?"
//     (LoggingRenderer dumps every RenderInput slot with source + HH:MM)
//   - "did it freeze, or did it just plan a huge sleep?"
//     (LoggingSleep records each requested duration before sleeping)

namespace loglabel {

inline const char *state(DisplayState s) {
  switch (s) {
  case DisplayState::Boot:
    return "Boot";
  case DisplayState::Normal:
    return "Normal";
  case DisplayState::Offline:
    return "Offline";
  case DisplayState::Auth:
    return "Auth";
  case DisplayState::WifiAuth:
    return "WifiAuth";
  }
  return "?";
}

inline const char *source(DepartureSource s) {
  switch (s) {
  case DepartureSource::Unknown:
    return "UNK";
  case DepartureSource::Realtime:
    return "RT";
  case DepartureSource::Plan:
    return "PLAN";
  case DepartureSource::Hint:
    return "HINT";
  }
  return "?";
}

} // namespace loglabel

class LoggingRenderer : public IRenderer {
public:
  LoggingRenderer(IRenderer &inner, RunLog &log) : inner_(inner), log_(log) {}

  void render(const RenderInput &in, Frame &fb) override {
    log_.line("[render] state=%s api_ok=%d", loglabel::state(in.state),
              in.snapshot.api_ok);
    for (int s = 0; s < STREAM_COUNT; ++s) {
      const StreamData &sd = in.snapshot.stream[s];
      char slots[128];
      int off = 0;
      for (int k = 0; k < SLOTS_PER_STREAM; ++k) {
        const Departure &d = sd.slot[k];
        if (d.valid) {
          struct tm local;
          localtime_r(&d.when, &local);
          off += std::snprintf(
              slots + off, sizeof(slots) - off, " %02d:%02d/%s%s%s",
              local.tm_hour, local.tm_min, loglabel::source(d.source),
              d.line_label[0] != '\0' ? "/" : "", d.line_label);
        } else {
          off += std::snprintf(slots + off, sizeof(slots) - off, " --:--");
        }
        if (off >= static_cast<int>(sizeof(slots)))
          break;
      }
      log_.line("[render]   stream[%d] r=%d f=%d%s", s, sd.endpoint_responded,
                sd.filter_matched, slots);
    }
    inner_.render(in, fb);
  }

private:
  IRenderer &inner_;
  RunLog &log_;
};

class LoggingDisplay : public IDisplay {
public:
  LoggingDisplay(IDisplay &inner, RunLog &log) : inner_(inner), log_(log) {}

  void drawFull(const uint8_t *fb) override {
    log_.line("[panel] drawFull");
    inner_.drawFull(fb);
  }
  void drawPartial(const uint8_t *fb, const Bbox &bbox) override {
    log_.line("[panel] drawPartial bbox=(%d,%d %dx%d)", bbox.x, bbox.y, bbox.w,
              bbox.h);
    inner_.drawPartial(fb, bbox);
  }
  void lightFull(const uint8_t *fb) override {
    log_.line("[panel] lightFull (ghost clear)");
    inner_.lightFull(fb);
  }
  void deepClean(const uint8_t *fb) override {
    log_.line("[panel] deepClean");
    inner_.deepClean(fb);
  }

private:
  IDisplay &inner_;
  RunLog &log_;
};

class LoggingSleep : public ISleep {
public:
  LoggingSleep(ISleep &inner, RunLog &log) : inner_(inner), log_(log) {}

  WakeCause wakeupCause() override { return inner_.wakeupCause(); }
  void deepSleep(unsigned seconds) override {
    log_.line("[sleep] deep %u s", seconds);
    inner_.deepSleep(seconds);
  }
  void lightSleep(unsigned seconds) override {
    log_.line("[sleep] light %u s", seconds);
    inner_.lightSleep(seconds);
  }

private:
  ISleep &inner_;
  RunLog &log_;
};

} // namespace bustaferl::native_runtime

#endif
