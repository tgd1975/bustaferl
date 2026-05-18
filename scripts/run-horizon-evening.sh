#!/usr/bin/env bash
# scripts/run-horizon-evening.sh — drive the ~5 h evening horizon test.
#
# Wraps `pio test -e longterm-horizon-evening` with three guarantees the
# bare Makefile target does not give:
#   1. Host stays awake for the full run (systemd-inhibit blocks idle,
#      suspend, and lid-switch — released the moment the test ends).
#   2. The full serial stream is teed into .tmp/traces/ so post-run
#      analysis can grep for HEAP/EFA/SLEEP/WAKE markers. The Makefile
#      target captures only the JSON summary, which is too thin for a
#      5 h iteration.
#   3. Pre-flight rejects a missing ESP32 instead of letting pio fail
#      ~30 s in with an opaque serial error.
#
# Usage:
#   scripts/run-horizon-evening.sh

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${REPO_ROOT}"

TMP_DIR=".tmp"
TRACE_DIR="${TMP_DIR}/traces"
JSON_OUT="${TMP_DIR}/longterm-horizon-evening.json"
TRACE_OUT="${TRACE_DIR}/longterm-horizon-evening.log"

# Pre-flight: ESP32 must be on USB. Pure shell-glob with nullglob so a
# non-matching pattern (e.g. no ttyACM*) just contributes zero entries
# instead of leaking a non-zero exit through `set -o pipefail`.
shopt -s nullglob
serial_devs=(/dev/ttyUSB* /dev/ttyACM*)
shopt -u nullglob
if [ "${#serial_devs[@]}" -eq 0 ]; then
    echo "ERROR: no ESP32 on /dev/ttyUSB*/ttyACM* — plug it in first." >&2
    exit 1
fi

# Pre-flight: systemd-inhibit and pio must be available.
for tool in systemd-inhibit pio tee; do
    if ! command -v "${tool}" >/dev/null 2>&1; then
        echo "ERROR: required tool not found: ${tool}" >&2
        exit 1
    fi
done

mkdir -p "${TRACE_DIR}"

echo "[horizon-evening] starting — ETA ~5 h"
echo "[horizon-evening] JSON  → ${JSON_OUT}"
echo "[horizon-evening] trace → ${TRACE_OUT}"
echo "[horizon-evening] sleep/suspend/lid-switch inhibited for the duration"

systemd-inhibit \
    --what=idle:sleep:handle-lid-switch \
    --who="bustaferl" \
    --why="longterm-horizon-evening (~5 h)" \
    --mode=block \
    bash -o pipefail -c "pio test -e longterm-horizon-evening -v --json-output-path '${JSON_OUT}' 2>&1 | tee '${TRACE_OUT}'"

echo "[horizon-evening] done."
