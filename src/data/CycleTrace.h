#ifndef BUSTAFERL_DATA_CYCLETRACE_H
#define BUSTAFERL_DATA_CYCLETRACE_H

#include <cstdint>

namespace bustaferl {

// Persistent event memory for the diagnostic mode (double-click screens).
// The device keeps no serial log across a reboot, so when the user spots an
// anomaly on the panel the only way to understand it is a compact history in
// RTC-slow memory. Two rings: one entry per cycle (what happened) and one
// entry per anomaly (rarer, kept separately so it does not scroll out of the
// cycle ring). Both are trivially copyable and sized against the RTC budget
// (docs/rtc-memory-budget.md): 16*12 + 16*6 + 4 = 292 B.

// How a cycle was triggered.
enum class CycleTrigger : std::uint8_t {
  Timer = 0,  // periodic wake / active-phase poll
  Button = 1, // manual short-press refresh
  ColdBoot = 2,
};

// Bit flags packed into CycleRecord::flags. Stream bits mirror the Stream
// enum order (0..3); the rest capture the cycle's outcome.
enum CycleFlag : std::uint16_t {
  CYC_STREAM0_OK = 1u << 0, // 58A -> Atzgersdorf produced a departure
  CYC_STREAM1_OK = 1u << 1, // 58A -> Hietzing
  CYC_STREAM2_OK = 1u << 2, // 58B -> Atzgersdorf
  CYC_STREAM3_OK = 1u << 3, // S-Bahn -> Hbf
  CYC_RENDERED = 1u << 4,   // frame was pushed (else last frame kept)
  CYC_RESCUE_TRIED = 1u << 5,
  CYC_RESCUE_OK = 1u << 6,
  CYC_STALE = 1u << 7,      // rendered the Stale screen
  CYC_DEEP_SLEEP = 1u << 8, // slept deep (else light/active)
};

// One cycle. 12 bytes.
struct CycleRecord {
  std::uint32_t at = 0;      // wall-clock epoch (0 = clock unsynced)
  std::uint16_t flags = 0;   // CycleFlag bitmask
  std::uint16_t sleep_s = 0; // planned sleep after this cycle
  std::uint8_t trigger = 0;  // CycleTrigger
  std::uint8_t failed_batches = 0;
  std::uint8_t retried_batches = 0;
  std::uint8_t heap_free_kb = 0; // free heap at cycle end, in kB (0 = unknown)
};

// Anomaly kinds worth surfacing separately.
enum class TraceError : std::uint8_t {
  WifiFail = 0,      // wifi connect failed
  HttpOgd = 1,       // OGD batch HTTP failure (detail = batch index)
  HttpOebb = 2,      // ÖBB POST HTTP failure
  ParseFail = 3,     // JSON parse failure (detail = stream/endpoint)
  OebbAuth = 4,      // HAFAS err != OK (auth drift)
  EfaFail = 5,       // schedule (EFA) fetch failure
  NtpFail = 6,       // NTP sync failed
  StaleEnter = 7,    // entered Stale
  StaleExit = 8,     // recovered from Stale
  Filter58bDead = 9, // 58B towards-filter dead streak tripped
  RleOverflow = 10,  // framebuffer RLE exceeded the hardcap
};

// One anomaly. 6 bytes.
struct ErrorRecord {
  std::uint32_t at = 0;    // wall-clock epoch
  std::uint8_t code = 0;   // TraceError
  std::uint8_t detail = 0; // context-specific (batch index, http/8, …)
};

constexpr int CYCLE_TRACE_CAP = 16;
constexpr int ERROR_TRACE_CAP = 16;

// Two rings + head/count bookkeeping. head is the next write slot; count is
// how many valid entries exist (saturates at CAP). Trivially copyable so it
// rides in RTC-slow memory next to PersistedMeta.
struct CycleTrace {
  CycleRecord cycles[CYCLE_TRACE_CAP] = {};
  ErrorRecord errors[ERROR_TRACE_CAP] = {};
  std::uint8_t cycle_head = 0;
  std::uint8_t cycle_count = 0;
  std::uint8_t error_head = 0;
  std::uint8_t error_count = 0;
};

} // namespace bustaferl

#endif
