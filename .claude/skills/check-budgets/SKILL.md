---
name: check-budgets
description: Verify the project's hard memory + storage budgets — definite-leak-free under valgrind, peak host-heap below the v2 ceiling, ESP32 flash + RAM headroom non-negative, and (informational) RTC-Slow-Memory budget. Use when the user asks to "check budgets", "verify no leaks", "measure peak heap", "is the firmware too big", or as the §9 Heap-Profiling gate before tagging v2.0.0. Do NOT invoke as a side-effect of an unrelated build/test failure — it costs ~5–10 minutes wall-clock.
---

# check-budgets

This is the §9 (Heap-Profiling) gate for v2 — and the project's
single-source-of-truth for the question "do we still have room to
breathe?" The skill runs four independent checks against four
different budgets and reports a punch list. Each check is gated
against a written ceiling — no "looks fine to me" judgement calls.

The four checks correspond to the four ways this firmware could run
out of room before it ever ships:

1. **Leaks** — `valgrind --leak-check=full` over the native runtime
   driver (10 cycles). The cycle loop must be leak-free across cycle
   boundaries; if it isn't, the device leaks the same memory every
   wake and crashes after N days uptime — invisible to host tests
   that exit cleanly after one pass.

2. **Peak host heap** — `valgrind --tool=massif` over 50 cycles.
   Plots the heap-allocation profile and reports the peak. v2's
   expectation (from §9.2) is *Peak-Heap < pre-v2-Peak + 8 KB* —
   the HAFAS response is the only new allocation class and is
   ~21 KB raw, but std::string buffers + ArduinoJson DOM should not
   add more than 8 KB sustained peak over the pre-v2 baseline. This
   approximates the device-side V3 risk (mbedtls fragmentation
   when free heap drops below ~50 KB).

3. **ESP32 flash + dynamic RAM** — `pio run -e esp32dev -t size`.
   Surfaces flash usage vs. partition size and BSS/RODATA vs. SRAM.
   Hard gates: flash < 95 % of partition, dynamic RAM < 80 % of
   SRAM. Soft gate at 90 % flash with a warning. Plus Font-Data
   sanity check (V10 risk: U8g2 PROGMEM growth).

4. **RTC-Slow-Memory** — informational only. Reads
   [docs/rtc-memory-budget.md](../../../docs/rtc-memory-budget.md)
   and prints the current accounting. There is no runtime
   measurement — RTC budget is computed by reading struct sizes and
   adding them up. Drift between code and the budget table is
   human-tracked. This step exists so the user is reminded to update
   the table when `Departure` / `PersistedMeta` change.

## When to use

- The user explicitly asks for budget verification, peak-heap, leak
  check, firmware size, or "is there room left".
- §9 Heap-Profiling gate before tagging a major release (v2.0.0
  invokes this; Tier 3 of the `release` skill should depend on it).
- After a feature lands that touches an allocation class — bigger
  JSON, new persistent struct field, additional font, etc.

## When NOT to use

- After an unrelated CI failure. This skill takes ~5–10 minutes; it
  is not a generic "clear hooks" lever.
- During an active development loop where you're iterating on code.
  Run it at decision points, not every commit.
- On a branch that won't merge — there is no point gating budgets on
  a throwaway branch.

## Prerequisites

Hard requirements (all standard project toolchain):

- `valgrind` — provides `memcheck` + `massif`.
- `ms_print` — ships with valgrind (Debian/Ubuntu `valgrind` package).
- `pio` (PlatformIO Core) — already required by the project.
- `g++`, `make`, `find` — already in the standard toolchain.

Do NOT preflight with `command -v` — that contradicts the project
policy "Tool direkt aufrufen, nur bei Fehlschlag reagieren"
(memory: `feedback-no-preventive-tool-check`). If a tool is missing,
the make target fails with a clear error; surface that and ask the
user to install (e.g. `sudo apt install valgrind`). Partial budget
reports are misleading, so stop on first failure rather than
skipping a check.

## Steps

1. **Leak check** — `make native-runtime-smoke`. This builds the
   native runtime driver (if needed), then runs 10 cycles under
   `valgrind --leak-check=full --show-leak-kinds=definite
   --errors-for-leak-kinds=definite`. Exit code 0 = clean. Non-zero
   = there is a definite leak; the log is at
   `.tmp/native-runtime/valgrind.log`. Read the "definitely lost"
   line out of the log and surface it verbatim in the report. Do
   not attempt to fix the leak as part of this skill — fixing leaks
   is its own task. Just gate.

2. **Peak host heap** — `make native-runtime-massif`. Runs 50 cycles
   under massif and writes the raw output to
   `.tmp/native-runtime/massif-v2.out` plus a human-readable
   ms_print report at `.tmp/native-runtime/massif-v2.txt`. Extract
   the peak snapshot's heap value (line starting with the snapshot
   marked `peak`). Compare against the pre-v2 ceiling:

   - **Pre-v2 baseline**: If `.tmp/native-runtime/massif-pre-v2.out`
     exists, read its peak as the baseline. If not, the skill
     informs the user that there is no baseline on file and the
     check is informational only — do not gate.
   - **Ceiling**: baseline + 8 KB. Above ceiling = FAIL with the
     two numbers and the delta.

   If massif crashes mid-run (rare; usually OOM in the host system),
   surface the failure and stop.

3. **ESP32 flash + RAM** — `make size`. Parse the PlatformIO output
   for the four totals:

   - `Flash: [....] X.X% (used N bytes from M bytes)` — flash hard
     gate at 95 %, soft warning at 90 %.
   - `RAM: [....] X.X% (used N bytes from M bytes)` — dynamic RAM
     hard gate at 80 %, soft warning at 70 %.

   Above the hard gate is FAIL. Between soft and hard is a WARN.
   Below soft is PASS. Surface the absolute numbers + percentage
   for both rows.

4. **RTC-Slow-Memory recap** — read
   [docs/rtc-memory-budget.md](../../../docs/rtc-memory-budget.md)
   and print the current ~7.3 KB / 8 KB headline. This step is
   purely informational — there is no automated drift check between
   code and the budget table. Remind the user to update it if
   `Departure` or `PersistedMeta` layouts changed since the last
   update.

5. **Report.** Punch list with PASS / WARN / FAIL per check, the
   absolute numbers, and the artifact path so the user can dig
   deeper:

   ```text
   Budget check (run YYYY-MM-DD HH:MM):

   1. Leaks ........................... PASS  (no definite leaks, 10 cycles)
                                              .tmp/native-runtime/valgrind.log
   2. Peak host heap .................. PASS  142 KB peak (baseline 138 KB, ceiling 146 KB)
                                              .tmp/native-runtime/massif-v2.txt
   3. ESP32 flash ..................... WARN  91.2 % (1195 KB / 1310 KB) — soft warning at 90 %
   3. ESP32 dynamic RAM ............... PASS  64.1 % (210 KB / 327 KB)
   4. RTC-Slow-Memory (info) .......... ~7.3 KB / 8 KB used, ~800 B reserve
                                              docs/rtc-memory-budget.md

   Overall: 1 warning, 0 failures. Safe to ship.
   ```

   If any FAIL, the overall verdict is "BLOCKED — do not ship". If
   only WARNs, "PROCEED with note". All PASS = "PASS — ship".

6. **Pre-v2 baseline capture**, if `.tmp/native-runtime/massif-pre-v2.out`
   is missing, offer (but do not auto-do) to capture it from `main`:
   the user would `git checkout main`, `make native-runtime-massif`,
   `mv .tmp/native-runtime/massif-v2.out
   .tmp/native-runtime/massif-pre-v2.out`, `git checkout -` to
   establish the baseline. Without it, check 2 is informational —
   not a gate.

## Why a skill and not a plain make target

The four checks have four different output formats (valgrind log,
massif report, PlatformIO size table, markdown section) and four
different gate thresholds. A single `make budget-check` could chain
them, but the punch-list synthesis + the WARN-vs-FAIL judgement is
prompt-side work — that's the skill's job. The make targets do the
mechanical part; this skill stitches them into a verdict the user
can act on.

## Artifacts

All outputs live under `.tmp/native-runtime/`:

- `valgrind.log` — leak check output, ~50 lines, kept across runs.
- `massif-v2.out` — raw massif data, ~100 KB, machine-readable.
- `massif-v2.txt` — ms_print human-readable report, ~5 KB.
- `massif-pre-v2.out` — optional baseline, captured from `main`.

`make clean` removes `.tmp/` entirely. If the baseline is wiped, the
peak-heap gate falls back to informational until re-captured.

## Don'ts

- Do not run the checks individually in isolation and call that
  "budget check". Partial reports invite "but the leak check passed"
  reasoning when the firmware is 99 % full.
- Do not adjust the ceiling values inline to make a FAIL pass. If
  the ceiling is wrong, the user adjusts it deliberately by editing
  this skill (with a sentence saying why).
- Do not run during a context where you have unstaged C++ edits
  pending — `native-runtime-massif` rebuilds the host binary, but
  unstaged changes are picked up too. Report what tree state was
  measured.
