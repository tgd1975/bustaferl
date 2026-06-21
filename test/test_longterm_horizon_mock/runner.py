#!/usr/bin/env python3
"""PC-driven horizon-mock runner (test-setup migration §9).

Three phases over nine fetch cycles, driven by wallclock:
    cycles 1..3  -> full departure set    -> live data, hint OFF
    cycles 4..6  -> empty departure list  -> EFA hint should activate
    cycles 7..9  -> full departure set    -> hint should deactivate

The device firmware lives in test_main.cpp and runs in env
`longterm-horizon-mock-firmware`. It is built with two build-defines:
    -DMOCK_API_BASE=\"http://<host>:<port>/monitor\"
    -DMOCK_INSECURE=1
so it skips TLS and routes its fetches to this runner. The runner
serves over plain HTTP (no certs required), reads serial via
`pio device monitor`, and asserts on the `[engine]` lines.

This is the public interface — pass/fail is decided here, not on the
device. See test/README.md for the test set context.

Wiring:
    .tmp/horizon-mock-runner.log     full serial transcript
    .tmp/horizon-mock-result.json    pass/fail summary
"""
from __future__ import annotations

import argparse
import json
import os
import re
import socket
import subprocess
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
TMP_DIR = REPO_ROOT / ".tmp"
LOG_PATH = TMP_DIR / "horizon-mock-runner.log"
RESULT_PATH = TMP_DIR / "horizon-mock-result.json"

# How long the device waits between fetches, mirrored by the firmware.
CYCLE_INTERVAL_S = 60
TOTAL_CYCLES = 9


# --- mock server -----------------------------------------------------------

class _State:
    phase = "full"   # one of "full" | "empty"
    requests = 0


def _full_body() -> bytes:
    """Minimal Wiener-Linien monitor JSON with one departure per RBL."""
    payload = {
        "message": {"value": "OK"},
        "data": {
            "monitors": [
                {
                    "locationStop": {"properties": {"name": str(rbl)}},
                    "lines": [
                        {
                            "name": line,
                            "towards": towards,
                            "departures": {
                                "departure": [
                                    {
                                        "departureTime": {
                                            "timePlanned":
                                                "2026-05-17T18:00:00.000Z",
                                            "timeReal":
                                                "2026-05-17T18:00:00.000Z",
                                            "countdown": 5,
                                        }
                                    }
                                ]
                            },
                        }
                    ],
                }
                for rbl, line, towards in [
                    (8131, "58A", "Bhf. Atzgersdorf"),
                    (3757, "58A", "Hietzing U"),
                    (8132, "58B", "Bhf. Atzgersdorf"),
                ]
            ]
        },
    }
    return json.dumps(payload).encode("utf-8")


def _empty_body() -> bytes:
    payload = {"message": {"value": "OK"}, "data": {"monitors": []}}
    return json.dumps(payload).encode("utf-8")


class _Handler(BaseHTTPRequestHandler):
    def do_GET(self):  # noqa: N802 (BaseHTTPRequestHandler API)
        _State.requests += 1
        body = _full_body() if _State.phase == "full" else _empty_body()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *_args, **_kwargs):
        pass  # quiet


def _start_server(port: int) -> HTTPServer:
    srv = HTTPServer(("0.0.0.0", port), _Handler)
    t = threading.Thread(target=srv.serve_forever, daemon=True)
    t.start()
    return srv


def _lan_ip() -> str:
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("8.8.8.8", 80))
        return s.getsockname()[0]
    finally:
        s.close()


# --- pio orchestration -----------------------------------------------------

def _build_and_upload(host: str, port: int) -> None:
    base = f"http://{host}:{port}/monitor?activateTrafficInfo=stoerunglang"
    env_flag = (
        "-DMOCK_API_BASE=\\\"" + base + "\\\" -DMOCK_INSECURE=1"
    )
    env_extra = os.environ.copy()
    env_extra["PLATFORMIO_BUILD_FLAGS"] = env_flag
    cmd = [
        "pio", "run", "-e", "longterm-horizon-mock-firmware",
        "-t", "upload",
    ]
    subprocess.run(cmd, cwd=REPO_ROOT, env=env_extra, check=True)


# --- log capture -----------------------------------------------------------

ENGINE_LINE = re.compile(
    r"\[engine\]\s+cycle=(?P<cycle>\d+)\s+"
    r"realtime_count=(?P<rt>\d+)\s+"
    r"hint_active=(?P<hint>[01])"
)


def _read_serial_until_done() -> list[dict]:
    """Stream `pio device monitor`, collect `[engine]` lines, stop after cycle N."""
    proc = subprocess.Popen(
        ["pio", "device", "monitor", "-b", "115200"],
        cwd=REPO_ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        bufsize=1, text=True,
    )
    seen: list[dict] = []
    LOG_PATH.write_text("")
    deadline = time.monotonic() + (TOTAL_CYCLES + 2) * CYCLE_INTERVAL_S + 60
    try:
        with LOG_PATH.open("a") as log:
            assert proc.stdout is not None
            for raw in proc.stdout:
                log.write(raw)
                log.flush()
                m = ENGINE_LINE.search(raw)
                if m:
                    seen.append({
                        "cycle": int(m["cycle"]),
                        "realtime_count": int(m["rt"]),
                        "hint_active": m["hint"] == "1",
                    })
                if seen and seen[-1]["cycle"] >= TOTAL_CYCLES:
                    break
                if time.monotonic() > deadline:
                    break
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()
    return seen


# --- assertions ------------------------------------------------------------

def _assert(rows: list[dict]) -> list[str]:
    """Return list of failure messages; empty list means pass."""
    errs: list[str] = []
    by_cycle = {r["cycle"]: r for r in rows}
    for c in range(1, TOTAL_CYCLES + 1):
        if c not in by_cycle:
            errs.append(f"missing [engine] line for cycle={c}")
    if errs:
        return errs

    # Phase 1: cycles 1..3 — realtime present, hint OFF
    for c in (1, 2, 3):
        r = by_cycle[c]
        if r["realtime_count"] < 3:
            errs.append(
                f"phase1 cycle={c}: expected realtime_count >= 3, "
                f"got {r['realtime_count']}"
            )
        if r["hint_active"]:
            errs.append(f"phase1 cycle={c}: hint should be OFF")

    # Phase 2: cycles 4..6 — realtime empty, hint ON (by cycle 5 at latest)
    for c in (4, 5, 6):
        r = by_cycle[c]
        if r["realtime_count"] > 0:
            errs.append(
                f"phase2 cycle={c}: expected realtime_count == 0, "
                f"got {r['realtime_count']}"
            )
    if not by_cycle[6]["hint_active"]:
        errs.append("phase2: hint never activated by cycle 6")

    # Phase 3: cycles 7..9 — realtime back, hint OFF by cycle 9
    for c in (7, 8, 9):
        r = by_cycle[c]
        if r["realtime_count"] < 3:
            errs.append(
                f"phase3 cycle={c}: expected realtime_count >= 3, "
                f"got {r['realtime_count']}"
            )
    if by_cycle[9]["hint_active"]:
        errs.append("phase3: hint still active at cycle 9 — did not clear")

    return errs


# --- phase scheduler -------------------------------------------------------

def _phase_scheduler() -> None:
    """Switch _State.phase as cycles progress.

    The firmware ticks at CYCLE_INTERVAL_S. We move slightly EARLIER than
    each boundary to make sure the next request hits the new phase.
    """
    settle = 5  # seconds before each fetch the firmware actually does it
    time.sleep(3 * CYCLE_INTERVAL_S - settle)
    _State.phase = "empty"
    time.sleep(3 * CYCLE_INTERVAL_S)
    _State.phase = "full"


# --- main ------------------------------------------------------------------

def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=8080)
    ap.add_argument("--skip-build", action="store_true",
                    help="assume firmware already uploaded with right defines")
    args = ap.parse_args(argv)

    TMP_DIR.mkdir(parents=True, exist_ok=True)
    host = _lan_ip()
    print(f"[runner] LAN IP {host}, port {args.port}, total {TOTAL_CYCLES} "
          f"cycles ≈ {TOTAL_CYCLES * CYCLE_INTERVAL_S}s")

    srv = _start_server(args.port)
    threading.Thread(target=_phase_scheduler, daemon=True).start()

    try:
        if not args.skip_build:
            _build_and_upload(host, args.port)
        rows = _read_serial_until_done()
    finally:
        srv.shutdown()

    errs = _assert(rows)
    result = {
        "host": host,
        "port": args.port,
        "rows": rows,
        "requests_served": _State.requests,
        "errors": errs,
        "pass": not errs,
    }
    RESULT_PATH.write_text(json.dumps(result, indent=2))
    if errs:
        print("[runner] FAIL:")
        for e in errs:
            print("  -", e)
        return 1
    print(f"[runner] PASS — {len(rows)} cycles, "
          f"{_State.requests} requests served")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
