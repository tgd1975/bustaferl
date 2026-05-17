#!/usr/bin/env bash
# SessionStart hook: injects the core rules of the three project-policy
# skills (bash-no-prompts, check-tool, fix-markdown) so the model follows
# them without needing explicit invocation. Listing a skill via
# skillOverrides only makes its NAME visible; this hook makes its RULES
# part of the session context.
set -euo pipefail

read -r -d '' CTX <<'EOF' || true
PROJECT POLICY (bustaferl) — these three skills are always-on; follow their guidance proactively without needing /invocation:

1) bash-no-prompts: NEVER chain bash commands with &&, ;, || in a single Bash call — each chained string is a separate allowlist miss = permission prompt. Split into separate Bash calls (parallel when independent, sequential when dependent). Use Read/Edit/Grep instead of head/tail/sed/awk/cat/echo. Drop diagnostic suffixes like `; echo "EXIT=$?"`. The allowlist matches WHOLE command strings — every mismatch costs the user attention.

2) check-tool: Before invoking a CLI tool that may not be installed (especially anything outside the standard toolchain), verify availability with `command -v <tool>` first. If missing, ask the user to install it before continuing — don't silently fail or work around it.

3) fix-markdown: When you edit any .md file, run `markdownlint-cli2 --fix` over the changed files afterward and report what was auto-fixed. Markdown-lint is already permitted in this project; run it as part of finishing markdown work.
EOF

jq -n --arg ctx "$CTX" \
  '{hookSpecificOutput:{hookEventName:"SessionStart",additionalContext:$ctx}}'
