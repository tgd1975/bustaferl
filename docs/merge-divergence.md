# Merge divergence: `v2/sbahn-atzgersdorf` ↔ `origin/main`

## Topology

Common ancestor: **`3480f5d`** — a *v1-era* commit (only touches the Makefile).
**Both branches built "v2 S-Bahn" independently from that same v1 base.**

```text
                      053a0e2 v2 S-Bahn backend ──► rescue-fetch ──► 7f9d871  origin/main
                     ╱  (claude/* review line: OverlayKind + banners)
3480f5d (v1) ───────┤
                     ╲  Schritt 1..11 v2 build ──► DisplayState + HostCanvas ──► e720466  HEAD
                        (this branch)
```

They are **two parallel v2 implementations**, not a feature branch off main. That's why 46 files conflict.

## The scale asymmetry (git diff HEAD→origin/main)

`src/` totals **+868 / −3007**. The 3007 "deletions" going HEAD→main = code that **exists only on this branch**:

| Only on this branch (main lacks entirely) | Only on main (this branch lacks) |
|---|---|
| `canvas.*`, `canvas_adafruit.*`, `canvas_host.*` (HostCanvas pixel-parity) | `rescue_policy.{cpp,h}` (~63 lines) — rescue-fetch |
| `badge.*`, `network_plan.*`, `plan_marker.*`, `custom_glyph.*` | `oebb_http_ok` flag + ÖBB auth-streak banner |
| `display_state.*` + `selectDisplayState` (7-state model) | `filter_dead_58b` / `oebb_auth_dead` inline banners |
| `rle.*`, 90px glyph assets, U8g2 integration | |

**This branch is ~3000 lines and several architectural steps ahead.** main carries ~one feature (rescue-fetch) and two inline banners this branch never got.

## Per-subsystem divergence

### 1. Display / render pipeline — **incompatible signatures**

- **HEAD:** `DisplayState` enum (Boot/Auth/Offline/Stale/Quiet/Night/Normal) + `selectDisplayState(snap, schedule, meta, sig)`; `composeRenderInput(state, snap, schedule, meta, now)`.
- **main:** `OverlayKind` (None/Stale) + `composeRenderInput(snap, schedule, overlay, now)` — no DisplayState, no SelectorSignals at all.
- **Relationship:** HEAD is a **strict superset** — main's Stale-overlay is just one of HEAD's 7 states. main has *nothing* HEAD's model can't express.
- **Recommendation:** **keep HEAD.**

### 2. Fetch / snapshot_fetcher — **HEAD newer, but main has one flag HEAD needs**

- **HEAD:** finalized topology (`OGD_FETCH_COUNT = 3`, S-Bahn out-of-band), auth-tripwire persisted in `meta`, §9 heap/sleep instrumentation in `FetchSummary`, `FetchInputs` bundle.
- **main:** still comments "5 streams / U1 Oberlaa"; `OGD_STREAM_COUNT = STREAM_SBAHN_HBF`; simpler `FetchSummary` — **but** uniquely has `oebb_http_ok` (needed to tell "no S-Bahn response" from "auth error").
- **Recommendation:** **keep HEAD**, port only `oebb_http_ok` **if** we want main's auth-banner.

### 3. Rescue-fetch (`rescue_policy`, `nextRescueStep`) — **main-only feature, genuinely new**

- Re-fetches incomplete snapshots in a 20–40 s window after a display update so half-empty boards (`--:--` on a running line) self-heal. Self-contained, ~63 lines, clean host-testable design.
- **This is the one thing worth pulling forward.** It's orthogonal to the DisplayState refactor.
- **Recommendation:** **port onto HEAD** as a follow-up (needs `FetchSummary::oebb_http_ok` + `fetchComplete`).

### 4. Auth / filter inline banners (`filter_dead_58b`, `oebb_auth_dead`) — **design question**

- main renders per-section "filter dead / auth dead" banners. HEAD folded these into the `Auth`/`Stale` DisplayStates instead (see cycle_runner comment: *"subsumed by Stale/Quiet/Auth"*).
- **Recommendation:** **keep HEAD's approach**; adopt banners only if product wants them shown per-section rather than as a whole-screen state. Not a blocker.

### 5. Parsers (`oebb_hafas_parse.*` add/add), config.h, HAL, docs, tests

- Mechanical topology drift (4-stream naming, dedup, comments). HEAD's are newer (dedup same-minute journeys `dfe593c`, locality names). **Recommendation: keep HEAD**, spot-check nothing regressed.

## Bottom line

This is **not a merge, it's a reconciliation where HEAD is the clear baseline.** main contributes exactly one feature worth keeping (rescue-fetch) plus one supporting flag (`oebb_http_ok`) and an optional banner style.

### Recommended path

1. **Merge with `-X ours`** (or `git restore --source=HEAD` per conflicted file) to take HEAD's design wholesale, recording main's ancestry so history is honest.
2. **Cherry-pick rescue-fetch** (`rescue_policy` + `oebb_http_ok` + `fetchComplete`) onto HEAD as a **separate, reviewed follow-up commit** — not buried in the merge.
3. `make ci` gate before anything lands.

The alternative (hand-merge 46 files) risks silently dropping HostCanvas/DisplayState work with no green-build signal that anything broke — high effort, high risk, no upside over path 1.

### Open question for you

Do you actually want `origin/main` merged into this branch at all? Since HEAD supersedes it, the only *content* gain is rescue-fetch — which is better cherry-picked. A full merge mainly buys **shared history** (so a later `this → main` PR is a clean fast-forward). If you don't need that yet, the cleanest move is: **cherry-pick rescue-fetch, skip the merge.**
