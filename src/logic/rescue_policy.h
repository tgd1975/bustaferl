#ifndef BUSTAFERL_RESCUE_POLICY_H
#define BUSTAFERL_RESCUE_POLICY_H

#include <cstdint>
#include <ctime>

namespace bustaferl {

// Rescue window relative to the last display update: re-fetch attempts for an
// incomplete snapshot happen no earlier than window_start_s and no later than
// window_end_s after it. The lower bound keeps two screen updates from landing
// back-to-back; the upper bound hands over to the next regular poll cycle.
constexpr int DEFAULT_RESCUE_WINDOW_START_S = 20;
constexpr int DEFAULT_RESCUE_WINDOW_END_S = 40;
// Pause between two rescue attempts inside the window.
constexpr int DEFAULT_RESCUE_RETRY_PAUSE_S = 5;
// Upper bound on rescue fetches per cycle (each fetch retries per batch on
// its own via api_fetcher).
constexpr int DEFAULT_RESCUE_MAX_ATTEMPTS = 3;

struct RescueConfig {
  int window_start_s = DEFAULT_RESCUE_WINDOW_START_S;
  int window_end_s = DEFAULT_RESCUE_WINDOW_END_S;
  int retry_pause_s = DEFAULT_RESCUE_RETRY_PAUSE_S;
};

enum class RescueStep : std::uint8_t {
  Wait,  // before the window — sleep `wait_s`, then re-evaluate
  Retry, // inside the window — attempt a fetch now
  Stop,  // past the window (or clock anomaly) — give up until next cycle
};

// Decide what a rescue loop should do at `now`, given the display was last
// updated at `anchored_at`. On Wait, `wait_s` receives the seconds until the
// window opens; otherwise it is set to 0. `now < anchored_at` (clock jumped
// backwards) yields Stop — better to skip one rescue than to sleep on a bogus
// delta.
RescueStep nextRescueStep(time_t now, time_t anchored_at,
                          const RescueConfig &cfg, unsigned &wait_s);

} // namespace bustaferl

#endif
