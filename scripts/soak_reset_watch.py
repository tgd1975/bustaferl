#!/usr/bin/env python3
"""
Watch the production firmware's serial output over an extended, unattended
run and report every unplanned reset (brownout/watchdog/panic) — the
overnight symptom host tests (test_longterm_*) cannot see, because a real
chip reset kills the Unity test process along with the rest of RAM.

Every setup() call logs "[boot] cold" or "[boot] warm" (main.cpp), and
showBrownoutScreen() additionally logs "[boot] unplanned reset, reason=N"
whenever ISleep::lastResetReason() is not Normal (logic/cycle_runner.cpp).
This script mirrors every serial line — timestamped — into a log file and
counts:
  - unplanned resets (the "[boot] unplanned reset" line),
  - total boots (every "[boot] cold"/"[boot] warm" line),
  - the longest silent gap between two lines (a reset that happened to
    fall between two boot-log lines still shows up as a gap much longer
    than the configured poll interval).

Usage:
    scripts/soak_reset_watch.py --port /dev/ttyUSB0 --hours 8
    scripts/soak_reset_watch.py --port /dev/ttyUSB0 --hours 8 --gap-threshold-s 120

Exits non-zero (and prints a summary) if any unplanned reset was seen, or a
silent gap exceeded --gap-threshold-s. Runs until --hours elapses or Ctrl-C.
"""
from __future__ import annotations

import argparse
import re
import sys
import time
from datetime import datetime, timedelta
from pathlib import Path

import serial

DEFAULT_LOG = Path(".tmp/traces/soak-reset-watch.log")
BOOT_RE = re.compile(r"\[boot\] (cold|warm)\b")
UNPLANNED_RESET_RE = re.compile(r"\[boot\] unplanned reset, reason=(\d+)")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", required=True, help="serial device, e.g. /dev/ttyUSB0")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--hours", type=float, default=8.0, help="how long to watch")
    ap.add_argument("--gap-threshold-s", type=float, default=180.0,
                    help="a silent gap longer than this between two lines is flagged")
    ap.add_argument("--log", type=Path, default=DEFAULT_LOG, help="mirrored log file path")
    args = ap.parse_args()

    args.log.parent.mkdir(parents=True, exist_ok=True)
    deadline = datetime.now() + timedelta(hours=args.hours)

    boots = 0
    unplanned_resets = 0
    longest_gap_s = 0.0
    last_line_at = datetime.now()

    print(f"[watch] port={args.port} baud={args.baud} hours={args.hours} "
          f"log={args.log}")

    with args.log.open("w", buffering=1) as log_f, \
         serial.Serial(args.port, args.baud, timeout=1) as ser:
        try:
            while datetime.now() < deadline:
                raw = ser.readline()
                now = datetime.now()
                if not raw:
                    gap_s = (now - last_line_at).total_seconds()
                    if gap_s > longest_gap_s:
                        longest_gap_s = gap_s
                    continue

                last_line_at = now
                line = raw.decode("utf-8", errors="replace").rstrip("\r\n")
                stamped = f"{now.isoformat(timespec='seconds')} {line}"
                log_f.write(stamped + "\n")

                if BOOT_RE.search(line):
                    boots += 1
                m = UNPLANNED_RESET_RE.search(line)
                if m:
                    unplanned_resets += 1
                    print(f"[watch] UNPLANNED RESET #{unplanned_resets} "
                          f"reason={m.group(1)} at {stamped}")
        except KeyboardInterrupt:
            print("[watch] interrupted, finishing up")

    print(f"[watch] DONE: boots={boots} unplanned_resets={unplanned_resets} "
          f"longest_silent_gap={longest_gap_s:.0f}s log={args.log}")

    gap_exceeded = longest_gap_s > args.gap_threshold_s
    if unplanned_resets > 0 or gap_exceeded:
        print("[watch] FAIL: unplanned reset(s) and/or a silent gap "
              f"beyond --gap-threshold-s={args.gap_threshold_s:.0f}s")
        return 1
    print("[watch] OK: no unplanned resets, no excessive silent gap")
    return 0


if __name__ == "__main__":
    sys.exit(main())
