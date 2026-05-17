---
name: release
description: Cut a new tagged release of bustaferl. Enforces the Tier 2/3 test-gate (host + device + soak), composes release notes from git log, and creates an annotated tag. Use when the user wants to "tag a release", "ship v1.x", "cut a release", or asks to verify pre-tag readiness. Optional `--major` flag adds the 24h test gate. Refuses if tests are stale or rot, blocks force-push to main.
---

# bustaferl release skill

## When to invoke

- User: "release this", "tag a new version", "cut v1.2.3", "/release", "/release --major"
- After a feature lands and the user is ready to ship, do NOT proactively suggest — wait for explicit ask. Releases are deliberate, not automated.

## Refuse to run if

- Working tree is dirty (`git status` not empty) — uncommitted work means the tag would not match what's being tested
- HEAD is on `main` and the user is asking for `--major` (push to main is denied at the hook level anyway, but warn early)
- Plan/spec docs in the working tree show "TODO" markers added for this release that aren't resolved

## Inputs

- `--major`: also require `test-longterm-day-full` JSON to be green for current HEAD
- `--minor` or `--patch` (optional): pre-specifies the bump kind; otherwise ask interactively
- `--push`: after tag creation, push the tag to origin (requires explicit confirmation)

## Steps

### 1. Pre-flight

```bash
git status --porcelain                  # must be empty
git rev-parse HEAD                      # remember this — it's the SHA the gate checks
git describe --tags --abbrev=0 || true  # last tag (may be missing on first release)
```

If working tree is dirty, refuse with `[release] working tree has uncommitted changes — commit or stash first`.

### 2. Tier 2 Test-Gate

The `make test-all` and `make test-longterm-soak-15min` targets each write a sidecar metadata file alongside the JSON:

```text
.tmp/test-all.meta.json              {"commit": "<sha>", "timestamp": <epoch>, "pass": <bool>}
.tmp/longterm-soak-15min.meta.json   same shape
```

Read both. For each:

- If file missing → block: `[release] no prior 'make test-all' run found — run it first`
- If `commit != current HEAD` → block: `[release] 'make test-all' was run on <old-sha>, not on current HEAD <new-sha> — re-run`
- If `pass == false` → block: `[release] 'make test-all' did not pass on <sha>`
- If `timestamp` older than 24 h → warn (do not block) — release tests aged but still match SHA

With `--major`: also gate on `.tmp/longterm-day-full.meta.json` the same way.

### 3. Version bump

If kind not provided: present the last tag and ask `patch / minor / major`. Then compute the new version by SemVer rules. There is no separate VERSION file in this repo — the tag IS the version.

### 4. Release notes

```bash
git log <last-tag>..HEAD --no-merges --pretty='format:- %s'
```

Group by leading keyword (Feat:/Fix:/Tooling: etc.) if present. Show the draft to the user, accept inline edits before tagging.

### 5. Create tag

```bash
git tag -a v<new-version> -m "<release notes message>"
```

The body of the tag message IS the release notes from step 4. Annotated tag (not lightweight) so `git log` and the tag itself stay linked.

### 6. Push (only if --push)

Confirm with the user: `[release] push tag v<new-version> to origin? (y/N)`. On yes:

```bash
git push origin v<new-version>
```

NEVER `git push origin main` from this skill — only the tag ref.

## Pass criteria

- Tests gated by SHA, not just file presence → release proves the tested code is the tagged code
- Tag creation refused on stale/red tests
- Release notes are user-edited, not autosubmitted
- Token "ich vergess das vor dem Tag" failure mode is eliminated by the gate

## Failure modes & remedies

| Symptom | Cause | Fix |
| --- | --- | --- |
| `no prior 'make test-all' run found` | Tests never ran or `.tmp/` was wiped | `make test-all` then retry |
| `commit mismatch` | New commits landed after the last test run | re-run `make test-all` on HEAD |
| `did not pass` | Tests failed | fix the failure, re-run, then retry |
| `make test-longterm-soak-15min` skipped | No hardware attached | release blocks; this is correct — you cannot ship without the heap-leak check |

## Phase-4 prerequisites (already wired in by test-setup-migration)

- `.tmp/` is the canonical scratch dir (see [[scratch-files-go-to-tmp]] memory)
- `make test-all` and `make test-longterm-soak-*` already write JSON results to `.tmp/` via `--json-output-path`
- Sidecar `.meta.json` files (with commit SHA + timestamp + pass) are written by the Makefile after each gated test target
