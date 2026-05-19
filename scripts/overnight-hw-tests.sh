#!/usr/bin/env bash
#
# HW-test runner: fires the hardware-side gates the refactor branch still
# needs before merge, sequentially, hands-off.
#
# Self-wraps in systemd-inhibit on first entry so the run survives
# idle-suspend, lid-close, and battery-handover. The inhibit is released
# automatically when this script exits.
if [ -z "${OVERNIGHT_INHIBITED:-}" ]; then
  export OVERNIGHT_INHIBITED=1
  exec systemd-inhibit \
    --what=sleep:idle:handle-lid-switch:handle-suspend-key:handle-hibernate-key \
    --who=bustaferl-overnight-tests \
    --why="HW-test gates (Schritt 8)" \
    --mode=block \
    bash "$0" "$@"
fi
#
# Order: device tests first (quickest + fail-fast), smoke (~3 min), then
# the 15-min soak. horizon-evening is NOT in this set — it needs its own
# evening run window (tm_hour >= 20 || tm_hour <= 3 pre-condition).
#
# Each step gets a hard outer timeout. PIO 6.1.19 doesn't enforce
# test_timeout, and a crashing on-device test (e.g. a misconfigured
# Unity build) reboots the ESP32 forever — last run that ate ~8 h on
# test_device_persistent's UNITY_SUPPORT_64 reset loop. The timeout
# caps that to its budget and the next step still runs.
#
# Per-step budgets (real expected runtimes in parens):
#   test-device              45 min  (~5 min for 5 device envs)
#   test-longterm-smoke      10 min  (~3 min)
#   test-longterm-soak-15min 25 min  (~15 min plus build/flash)
#
# Each test's full output goes to .tmp/overnight/<name>.log. A combined
# status table is written to .tmp/overnight/summary.txt at the end.
#
# Bail-out policy: each step is independent. A failure (or timeout) is
# recorded in summary.txt; subsequent steps still run.

set -u  # no -e: a failing test must not abort subsequent runs

cd "$(dirname "$0")/.."
OUT=.tmp/overnight
mkdir -p "$OUT"

run() {
  local label="$1" budget="$2"; shift 2
  local logfile="$OUT/${label}.log"
  printf '%s  START  %s  budget=%ss\n' \
    "$(date '+%F %T')" "$label" "$budget" | tee -a "$OUT/summary.txt"
  local t0
  t0=$(date +%s)
  timeout --kill-after=30 "$budget" "$@" >"$logfile" 2>&1
  local rc=$?
  local t1
  t1=$(date +%s)
  local dt=$(( t1 - t0 ))
  local verdict="PASS"
  if [ $rc -eq 124 ] || [ $rc -eq 137 ]; then
    verdict="TIMEOUT"
  elif [ $rc -ne 0 ]; then
    verdict="FAIL"
  fi
  printf '%s  %-7s  %-30s  %d s  rc=%d  log=%s\n' \
    "$(date '+%F %T')" "$verdict" "$label" "$dt" "$rc" "$logfile" \
    | tee -a "$OUT/summary.txt"
}

: > "$OUT/summary.txt"
printf '=== HW-test run started %s ===\n\n' \
  "$(date '+%F %T')" | tee -a "$OUT/summary.txt"

run test-device              2700 make test-device
run test-longterm-smoke       600 make test-longterm-smoke
run test-longterm-soak-15min 1500 make test-longterm-soak-15min

printf '\n=== HW-test run finished %s ===\n' \
  "$(date '+%F %T')" | tee -a "$OUT/summary.txt"
