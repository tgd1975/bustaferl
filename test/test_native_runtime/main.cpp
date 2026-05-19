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
#include "NoOpDisplay.h"
#include "NoOpSleep.h"
#include "RecordingRenderer.h"
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
  //   BUSTAFERL_INSECURE        — "1" → TLS verify off (for the mock runner)
  //   BUSTAFERL_TIME_SCALE      — sleep multiplier, default 1.0 (real seconds)
  //   BUSTAFERL_MAX_CYCLES      — stop after N warm cycles, 0 = unlimited
  //   BUSTAFERL_PERSIST_PATH    — disk-store file path
  //   BUSTAFERL_FRESH_BOOT      — "1" → delete persist file at startup
  CycleConfig cfg{};
  cfg.api_base = envOr("BUSTAFERL_API_BASE",
                       "https://www.wienerlinien.at/ogd_realtime/monitor");
  cfg.efa_base =
      envOr("BUSTAFERL_EFA_BASE",
            "https://www.wienerlinien.at/ogd_routing/XSLT_DM_REQUEST");

  const double time_scale = envDouble("BUSTAFERL_TIME_SCALE", 1.0);
  const unsigned max_cycles = envUint("BUSTAFERL_MAX_CYCLES", 0);
  const std::string persist_path =
      envOr("BUSTAFERL_PERSIST_PATH", ".tmp/native-runtime/persist.bin");
  const bool fresh_boot =
      std::string{envOr("BUSTAFERL_FRESH_BOOT", "1")} == "1";

  if (fresh_boot)
    std::remove(persist_path.c_str());

  WallClockClock clock;
  HttpsNet net;
  if (std::string{envOr("BUSTAFERL_INSECURE", "0")} == "1")
    net.setInsecure(true);
  NoOpSleep sleep{time_scale};
  DiskStore store{persist_path};
  NoOpDisplay display;
  RecordingRenderer renderer{".tmp/native-runtime"};

  Frame frame_new;
  Frame frame_prev;

  CycleDeps deps{clock,    net,       sleep,      store, display,
                 renderer, frame_new, frame_prev, cfg};

  PersistedMeta meta = store.loadMeta();

  std::fprintf(stderr, "[runtime] cold-boot cycle\n");
  runColdCycle(deps, meta);

  unsigned warm_cycles = 0;
  while (!g_should_exit.load(std::memory_order_relaxed)) {
    if (max_cycles > 0 && warm_cycles >= max_cycles)
      break;
    ++warm_cycles;
    std::fprintf(stderr, "[runtime] warm cycle %u\n", warm_cycles);
    meta = store.loadMeta();
    runWarmCycle(deps, meta);
  }

  std::fprintf(stderr,
               "[runtime] done — warm_cycles=%u render=%u dump=%u "
               "panel(full=%u/partial=%u/light=%u/deep=%u)\n",
               warm_cycles, renderer.render_count(), renderer.dump_count(),
               display.draw_full, display.draw_partial, display.light_full,
               display.deep_clean);
  return 0;
}
