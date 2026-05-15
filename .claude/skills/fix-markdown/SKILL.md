---
name: fix-markdown
description: Auto-fix markdown lint issues across all .md files and report what changed
---

# fix-markdown

Run `markdownlint-cli2 --fix` across every Markdown file in the repo
(matching the pre-commit hook's glob) and report what was changed.

## Parallel-session warning — read before invoking

This skill is **deliberately project-wide**. It edits every `.md` file
that has a fixable lint issue, not only files you intend to commit. In
a repo where multiple Claude Code sessions can run concurrently, that
sweep can clobber another session's working-tree edits if the fixer
touches a file they have open.

Only invoke this skill when:

- You explicitly want a tree-wide markdown cleanup as its own commit.
- You know no other session is mid-edit on a `.md` file.

Do **not** invoke it as a side-effect of another flow to clear an
unrelated hook failure — that is the anti-pattern the `/commit`
skill's three-check protocol exists to prevent. Touching files you
have no intent to commit violates the project's "commit only your own
work" rule.

## Steps

1. Record which `.md` files currently have unstaged changes — these
   are the *before* set, used in step 3 to detect what the fixer
   touched:

   ```bash
   git diff --name-only -- '*.md'
   ```

2. Run `markdownlint-cli2` in fix mode against the same glob the
   pre-commit hook uses, so the skill and the hook agree on scope:

   ```bash
   markdownlint-cli2 --fix '**/*.md' '!node_modules' '!.venv' '!.claude/security-review-latest.md'
   ```

   (There is no `make lint-markdown` target in this repo — earlier
   versions of this skill referenced one that did not exist.)

3. Re-run the `git diff --name-only -- '*.md'` from step 1 to get the
   *after* set. Files newly present in the after-set were modified by
   the fixer. Files already present in both sets may have had
   additional fixer-applied changes layered on top of pre-existing
   unstaged edits — disentangling that requires per-hunk inspection
   and is not part of this skill's job.

4. Report:
   - The list of newly-modified files (after-set minus before-set),
     one per line, under a clear heading.
   - "All Markdown files already lint-clean." if nothing changed.
   - Any files in both sets whose diff grew — note them, but do not
     try to attribute the growth.

5. If files were modified, remind the user to review the fixer's
   changes before staging them. Stage and commit via `/commit` per
   the project's commit protocol — never `git add` directly.

Do not stage or commit — leave that to the user.
