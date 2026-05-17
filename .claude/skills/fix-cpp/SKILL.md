---
name: fix-cpp
description: Run clang-format (auto-fix) across every C/C++ file in the repo and report what was changed; then run cppcheck and surface any remaining static-analysis warnings as a punch list. Mirrors fix-markdown — formatter auto-fixes, linter reports. Use when the user asks to "fix the C++ lint issues", "format the C++ code", or "run the C++ linters". Do NOT invoke as a side-effect to clear an unrelated hook failure.
---

# fix-cpp

Run `make format` (clang-format `-i` in place) across every `.h`/`.cpp`
file the project lints, then run `make lint` (cppcheck `--enable=warning,style`)
and report any remaining warnings. Formatter auto-fixes, linter only
reports — cppcheck issues need manual judgement (false-positive
suppression vs. real bug).

## Parallel-session warning — read before invoking

This skill is **deliberately project-wide**. It reformats every
`src/**/*.{h,cpp}` and `test/**/*.{h,cpp}` file that has a
clang-format violation, not only files you intend to commit. In a repo
where multiple Claude Code sessions can run concurrently, that sweep
can clobber another session's working-tree edits if the formatter
touches a file they have open.

Only invoke this skill when:

- You explicitly want a tree-wide C++ formatting cleanup as its own
  commit.
- You know no other session is mid-edit on a `.cpp`/`.h` file.

Do **not** invoke it as a side-effect of another flow to clear an
unrelated hook or CI failure — that is the anti-pattern the
`/commit` skill's three-check protocol exists to prevent. Touching
files you have no intent to commit violates the project's "commit
only your own work" rule.

## Steps

1. Record which C++ files currently have unstaged changes — these
   are the *before* set, used in step 3 to detect what the formatter
   touched:

   ```bash
   git diff --name-only -- '*.h' '*.cpp'
   ```

2. Run the project's clang-format target — it formats every
   `src/` and `test/` `.h`/`.cpp` in place using the project's
   `.clang-format` (BasedOnStyle: LLVM, 80-col):

   ```bash
   make format
   ```

   `make format` is silent on success. Failures (missing
   `clang-format` binary, parse errors) print to stderr and exit
   non-zero. If it fails, abort and surface the error — do not
   attempt to format file-by-file as a workaround.

3. Re-run `git diff --name-only -- '*.h' '*.cpp'` from step 1 to get
   the *after* set. Files newly present in the after-set were
   modified by the formatter. Files already present in both sets may
   have had additional formatter-applied changes layered on top of
   pre-existing unstaged edits — disentangling that requires per-hunk
   inspection and is not part of this skill's job.

4. Verify formatting is clean:

   ```bash
   make format-check
   ```

   Silent + exit 0 = good. Non-zero = a file the formatter could not
   bring into compliance (rare; usually a clang-format version skew).
   Surface the offending paths and stop — don't pretend success.

5. Run the project's cppcheck target:

   ```bash
   make lint
   ```

   This does NOT auto-fix; cppcheck only reports. Capture stderr.
   Exit 0 = clean. Exit non-zero = there are warnings; each one is
   one of:
   - a real bug (fix the code)
   - a false positive worth suppressing inline via
     `// cppcheck-suppress <id>` directly above the trigger line
     (project Makefile already passes `--inline-suppr`)
   - a class-of-warning the project tolerates → discuss with user
     whether to add `--suppress=<id>` to the Makefile target

   Either of the latter two is a project-policy decision — do not
   make it unilaterally. Report cppcheck warnings as a punch list and
   let the user decide.

6. Report:
   - The list of files newly modified by clang-format (after-set
     minus before-set), one per line, under a clear heading.
   - "All C++ files already clang-format clean." if nothing changed.
   - Any files in both sets whose diff grew — note them, but do not
     try to attribute the growth.
   - The cppcheck punch list with file:line, rule id, and a one-line
     summary per warning. "cppcheck clean." if exit 0.

7. If files were modified, remind the user to review the formatter's
   changes before staging them. Stage and commit via `/commit` per
   the project's commit protocol — never `git add` directly.

Do not stage or commit — leave that to the user.

## Why two tools, one skill

`clang-format` is a deterministic formatter — no judgement, no false
positives, safe to auto-fix. `cppcheck` is a static analyser — every
warning needs human judgement (fix code? suppress? change rule?). The
skill auto-applies the former and surfaces the latter so the work is
visible to the user without a second invocation.
