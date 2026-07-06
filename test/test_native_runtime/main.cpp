// Headless host runtime driver: runs runColdCycle once, then runWarmCycle in
// a loop until SIGINT/SIGTERM or the configured cycle count is reached.
// Counts cycles, dumps PGM frames into .tmp/native-runtime/, and exits 0 on
// clean shutdown.
//
// Build artefacts are produced by the Makefile target
// `native-runtime-smoke` (10 cycles, valgrind-wrapped) and
// `native-runtime-day` (24h). See test/test_native_runtime/README.md.

#include "../../src/logic/cycle_runner.h"
#include "../../src/render/layout.h"
#include "DiskStore.h"
#include "HttpsNet.h"
#include "LoggingAdapters.h"
#include "NoOpDisplay.h"
#include "NoOpSleep.h"
#include "RecordingRenderer.h"
#include "RunLog.h"
#include "WallClockClock.h"

#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

std::atomic<bool> g_should_exit{false};

void sigHandler(int /*sig*/) {
  g_should_exit.store(true, std::memory_order_relaxed);
}

const char *envOr(const char *name, const char *fallback) {
  const char *v = std::getenv(name);
  return (v != nullptr && v[0] != '\0') ? v : fallback;
}

double envDouble(const char *name, double fallback) {
  const char *v = std::getenv(name);
  if (v == nullptr || v[0] == '\0')
    return fallback;
  return std::atof(v);
}

unsigned envUint(const char *name, unsigned fallback) {
  const char *v = std::getenv(name);
  if (v == nullptr || v[0] == '\0')
    return fallback;
  return static_cast<unsigned>(std::strtoul(v, nullptr, 10));
}

} // namespace

int main() {
  using namespace bustaferl;
  using namespace bustaferl::native_runtime;

  std::signal(SIGINT, sigHandler);
  std::signal(SIGTERM, sigHandler);

  (void)!std::system("mkdir -p .tmp/native-runtime");

  // ENV-driven config:
  //   BUSTAFERL_API_BASE        — realtime endpoint, default = live production
  //   BUSTAFERL_EFA_BASE        — EFA schedule endpoint, default = live
  //   BUSTAFERL_MGATE_URL       — HAFAS mgate.exe URL, default = live; empty
  //                                 string skips the OEBB leg (host-only tests)
  //   BUSTAFERL_INSECURE        — "1" → TLS verify off (for the mock runner)
  //   BUSTAFERL_TIME_SCALE      — sleep multiplier, default 1.0 (real seconds)
  //   BUSTAFERL_MAX_CYCLES      — stop after N warm cycles, 0 = unlimited
  //   BUSTAFERL_PERSIST_PATH    — disk-store file path
  //   BUSTAFERL_FRESH_BOOT      — "1" → delete persist file at startup
  CycleConfig cfg{};
  // Default matches WL_API_BASE in config.h — the batch fetcher appends
  // "&stopId=…", so the base must already carry a query string. A bare
  // ".../monitor" yields "...monitor&stopId=…" and every bus stream comes
  // back endpoint_responded=false (S-Bahn unaffected — HAFAS is a POST).
  cfg.api_base =
      envOr("BUSTAFERL_API_BASE", "https://www.wienerlinien.at/ogd_realtime/"
                                  "monitor?activateTrafficInfo=stoerunglang");
  cfg.efa_base =
      envOr("BUSTAFERL_EFA_BASE",
            "https://www.wienerlinien.at/ogd_routing/XSLT_DM_REQUEST");
  cfg.mgate_url =
      envOr("BUSTAFERL_MGATE_URL", "https://fahrplan.oebb.at/bin/mgate.exe");

  const double time_scale = envDouble("BUSTAFERL_TIME_SCALE", 1.0);
  const unsigned max_cycles = envUint("BUSTAFERL_MAX_CYCLES", 0);
  const std::string persist_path =
      envOr("BUSTAFERL_PERSIST_PATH", ".tmp/native-runtime/persist.bin");
  const bool fresh_boot =
      std::string{envOr("BUSTAFERL_FRESH_BOOT", "1")} == "1";

  if (fresh_boot)
    std::remove(persist_path.c_str());

  // Every HAL interaction goes through the logging decorators into
  // run.log — timestamped and flushed per line, so a freeze shows up as a
  // gap after the last recorded action and the rendered slot values are on
  // record for every cycle (the data/representation boundary).
  RunLog log{envOr("BUSTAFERL_LOG_PATH", ".tmp/native-runtime/run.log")};

  WallClockClock clock;
  HttpsNet net;
  if (std::string{envOr("BUSTAFERL_INSECURE", "0")} == "1")
    net.setInsecure(true);
  NoOpSleep raw_sleep{time_scale};
  DiskStore store{persist_path};
  NoOpDisplay raw_display;
  RecordingRenderer raw_renderer{".tmp/native-runtime"};

  LoggingSleep sleep{raw_sleep, log};
  LoggingDisplay display{raw_display, log};
  LoggingRenderer renderer{raw_renderer, log};

  Frame frame_new;
  Frame frame_prev;

  CycleDeps deps{clock,    net,       sleep,      store, display,
                 renderer, frame_new, frame_prev, cfg};

  PersistedMeta meta = store.loadMeta();

  log.line("[cycle] cold boot (time_scale=%.2f max_cycles=%u)", time_scale,
           max_cycles);
  runColdCycle(deps, meta);

  unsigned warm_cycles = 0;
  while (!g_should_exit.load(std::memory_order_relaxed)) {
    if (max_cycles > 0 && warm_cycles >= max_cycles)
      break;
    ++warm_cycles;
    meta = store.loadMeta();
    log.line("[cycle] warm %u (partials=%u fb_valid=%d dumps=%u)", warm_cycles,
             meta.partial_count, meta.framebuffer_valid,
             raw_renderer.dump_count());
    runWarmCycle(deps, meta);
  }

  log.line("[cycle] done — warm_cycles=%u render=%u dump=%u "
           "panel(full=%u/partial=%u/light=%u/deep=%u)",
           warm_cycles, raw_renderer.render_count(), raw_renderer.dump_count(),
           raw_display.draw_full, raw_display.draw_partial,
           raw_display.light_full, raw_display.deep_clean);
  return 0;
}
