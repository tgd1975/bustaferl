// Recording fakes for the cycle_runner host tests (Tier 2 + Tier 3).
//
// Each fake records every method call as a string into a shared
// `std::vector<std::string>& trace`. Tier-2 tests then assert on the exact
// trace; Tier-3 tests look at the per-fake counters / latched arguments.
//
// Placed in test_native_cycle_runner_warm/ as the primary consumer; the
// cold-cycle and invariants test programs cross-include via relative path
// (same pattern as test_longterm_soak/RecordingDisplay.h vs.
// test_longterm_smoke/, see plan §0a.2-Annahme).

#ifndef BUSTAFERL_TEST_RECORDING_FAKES_H
#define BUSTAFERL_TEST_RECORDING_FAKES_H

#include "hal/IClock.h"
#include "hal/IDisplay.h"
#include "hal/INetwork.h"
#include "hal/IPersistentStore.h"
#include "hal/IRenderer.h"
#include "hal/ISleep.h"
#include "render/layout.h"

#include <cstring>
#include <string>
#include <vector>

namespace bustaferl {
namespace test {

inline std::string truncate(const std::string &s, std::size_t max) {
  return s.size() <= max ? s : s.substr(0, max) + "...";
}

class RecordingClock : public IClock {
public:
  RecordingClock(std::vector<std::string> &trace, time_t now, bool synced)
      : trace_(trace), now_(now), synced_(synced) {}
  time_t now() override {
    trace_.emplace_back("clock.now");
    return now_;
  }
  bool ntpSync() override {
    trace_.emplace_back("clock.ntpSync");
    ++ntp_sync_calls;
    synced_ = true;
    last_sync_ = now_;
    return true;
  }
  time_t lastSync() const override { return last_sync_; }
  bool isSynced() override {
    trace_.emplace_back(synced_ ? "clock.isSynced -> true"
                                : "clock.isSynced -> false");
    return synced_;
  }
  void advance(time_t delta) { now_ += delta; }
  void setNow(time_t t) { now_ = t; }
  int ntp_sync_calls = 0;

private:
  std::vector<std::string> &trace_;
  time_t now_;
  bool synced_;
  time_t last_sync_ = 0;
};

class RecordingNet : public INetwork {
public:
  RecordingNet(std::vector<std::string> &trace, bool wifi_ok, bool http_ok,
               std::string body)
      : trace_(trace), wifi_ok_(wifi_ok), http_ok_(http_ok),
        body_(std::move(body)) {}
  bool connect(unsigned timeout_ms) override {
    trace_.emplace_back("net.connect(" + std::to_string(timeout_ms) + ")");
    return wifi_ok_;
  }
  bool isConnected() override {
    trace_.emplace_back("net.isConnected");
    return wifi_ok_;
  }
  bool httpGet(const std::string &url, std::string &out) override {
    trace_.emplace_back("net.httpGet(" + truncate(url, 40) + ")");
    ++http_calls;
    if (!http_ok_)
      return false;
    out = body_;
    return true;
  }
  // ÖBB S-Bahn POST. Defaults to an empty-but-OK HAFAS body so the cycle's
  // auth-health stays green; tests that care set a richer body.
  bool httpPost(const std::string &url, const std::string &, const std::string &,
                std::string &out) override {
    trace_.emplace_back("net.httpPost(" + truncate(url, 40) + ")");
    ++http_post_calls;
    if (!http_ok_)
      return false;
    out = oebb_body_;
    return true;
  }
  void setOebbBody(std::string b) { oebb_body_ = std::move(b); }

private:
  std::vector<std::string> &trace_;
  bool wifi_ok_;
  bool http_ok_;
  std::string body_;
  std::string oebb_body_ = R"({"svcResL":[{"res":{"jnyL":[]}}],"err":"OK"})";

public:
  int http_calls = 0;
  int http_post_calls = 0;
};

class RecordingSleep : public ISleep {
public:
  RecordingSleep(std::vector<std::string> &trace, WakeCause cause)
      : trace_(trace), cause_(cause) {}
  WakeCause wakeupCause() override {
    trace_.emplace_back("sleep.wakeupCause");
    return cause_;
  }
  void deepSleep(unsigned seconds) override {
    trace_.emplace_back("sleep.deepSleep(" + std::to_string(seconds) + ")");
    ++deep_sleep_calls;
    last_deep_sleep_seconds = seconds;
  }
  void lightSleep(unsigned seconds) override {
    trace_.emplace_back("sleep.lightSleep(" + std::to_string(seconds) + ")");
    ++light_sleep_calls;
    last_light_sleep_seconds = seconds;
  }
  int deep_sleep_calls = 0;
  int light_sleep_calls = 0;
  unsigned last_deep_sleep_seconds = 0;
  unsigned last_light_sleep_seconds = 0;

private:
  std::vector<std::string> &trace_;
  WakeCause cause_;
};

class RecordingStore : public IPersistentStore {
public:
  explicit RecordingStore(std::vector<std::string> &trace) : trace_(trace) {}
  PersistedMeta loadMeta() override {
    trace_.emplace_back("store.loadMeta");
    return meta_;
  }
  void saveMeta(const PersistedMeta &m) override {
    trace_.emplace_back("store.saveMeta");
    ++save_meta_calls;
    meta_ = m;
  }
  size_t loadFramebuffer(uint8_t *out, size_t cap) override {
    trace_.emplace_back("store.loadFramebuffer");
    if (!fb_valid_ || cap < fb_.size())
      return 0;
    std::memcpy(out, fb_.data(), fb_.size());
    return fb_.size();
  }
  bool saveFramebuffer(const uint8_t *fb, size_t len) override {
    trace_.emplace_back("store.saveFramebuffer");
    fb_.assign(fb, fb + len);
    fb_valid_ = true;
    ++save_framebuffer_calls;
    return true;
  }
  ScheduleSnapshot loadSchedule() override {
    trace_.emplace_back("store.loadSchedule");
    return schedule_;
  }
  void saveSchedule(const ScheduleSnapshot &s) override {
    trace_.emplace_back("store.saveSchedule");
    schedule_ = s;
    ++save_schedule_calls;
  }
  void seedMeta(const PersistedMeta &m) { meta_ = m; }
  void seedSchedule(const ScheduleSnapshot &s) { schedule_ = s; }
  int save_meta_calls = 0;
  int save_framebuffer_calls = 0;
  int save_schedule_calls = 0;

private:
  std::vector<std::string> &trace_;
  PersistedMeta meta_;
  ScheduleSnapshot schedule_;
  std::vector<uint8_t> fb_;
  bool fb_valid_ = false;
};

class RecordingDisplay : public IDisplay {
public:
  explicit RecordingDisplay(std::vector<std::string> &trace) : trace_(trace) {}
  void drawFull(const uint8_t *) override {
    trace_.emplace_back("display.drawFull");
    ++draw_full_calls;
  }
  void drawPartial(const uint8_t *, const Bbox &) override {
    trace_.emplace_back("display.drawPartial");
    ++draw_partial_calls;
  }
  void lightFull(const uint8_t *) override {
    trace_.emplace_back("display.lightFull");
    ++light_full_calls;
  }
  void deepClean(const uint8_t *) override {
    trace_.emplace_back("display.deepClean");
    ++deep_clean_calls;
  }
  int draw_full_calls = 0;
  int draw_partial_calls = 0;
  int light_full_calls = 0;
  int deep_clean_calls = 0;

private:
  std::vector<std::string> &trace_;
};

class RecordingRenderer : public IRenderer {
public:
  explicit RecordingRenderer(std::vector<std::string> &trace) : trace_(trace) {}
  void render(const RenderInput &in, Frame &fb) override {
    trace_.emplace_back("renderer.render");
    ++calls;
    last_overlay = in.overlay;
    // Touch the framebuffer so a downstream refresh_planner-style diff against
    // the previous frame yields a non-empty change region.
    fb.clear(true);
    fb.setPixel(seq_ % Frame::width, (seq_ / Frame::width) % Frame::height,
                false);
    ++seq_;
  }
  int calls = 0;
  OverlayKind last_overlay = OverlayKind::None;

private:
  std::vector<std::string> &trace_;
  int seq_ = 0;
};

} // namespace test
} // namespace bustaferl

#endif
