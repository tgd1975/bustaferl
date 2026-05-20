#!/usr/bin/env python3
"""
PreToolUse hook: enforce the bash-no-prompts skill at the harness level.

Reads CC PreToolUse JSON on stdin. For Bash tool calls, blocks commands that
violate the bash-no-prompts policy:

  R1. Statement chaining (&&, ||, ;) outside quotes/heredocs.
  R2. Pipe-into-truncation: `| head -N` or `| tail -N`.
  R3. `echo` as a command primary (diagnostic echo).
  R4. `cat <file>` used to display a file (not heredoc, not stdin pipe).

Exit 2 + reason on stderr  -> block, reason fed back to Claude.
Exit 0                     -> allow (downstream allowlist still applies).

Source of truth: .claude/skills/bash-no-prompts/SKILL.md
"""
import json
import re
import sys


def mask_strings_and_heredocs(cmd: str) -> str:
    """Replace the *content* of single/double-quoted strings and heredoc bodies
    with 'X' so structural operators inside them are not detected. Quote and
    heredoc-marker characters are preserved so subsequent regexes can still
    locate the structure."""
    out = []
    i, n = 0, len(cmd)
    in_single = in_double = False
    while i < n:
        c = cmd[i]
        if in_single:
            out.append("'" if c == "'" else 'X')
            if c == "'":
                in_single = False
            i += 1
            continue
        if in_double:
            if c == '\\' and i + 1 < n:
                out.append('XX')
                i += 2
                continue
            if c == '"':
                in_double = False
                out.append('"')
                i += 1
                continue
            out.append('X')
            i += 1
            continue
        if c == "'":
            in_single = True
            out.append(c)
            i += 1
            continue
        if c == '"':
            in_double = True
            out.append(c)
            i += 1
            continue
        m = re.match(r"<<-?\s*(['\"]?)([A-Za-z_][A-Za-z0-9_]*)\1", cmd[i:])
        if m:
            marker = m.group(2)
            tag_end = i + m.end()
            out.append(cmd[i:tag_end])
            nl = cmd.find('\n', tag_end)
            if nl < 0:
                out.append(cmd[tag_end:])
                i = n
                continue
            out.append(cmd[tag_end:nl + 1])
            pos = nl + 1
            while pos < n:
                next_nl = cmd.find('\n', pos)
                end = next_nl if next_nl >= 0 else n
                line = cmd[pos:end]
                if line.strip() == marker:
                    out.append(line)
                    if next_nl >= 0:
                        out.append('\n')
                        pos = next_nl + 1
                    else:
                        pos = n
                    break
                out.append('X' * len(line))
                if next_nl >= 0:
                    out.append('\n')
                    pos = next_nl + 1
                else:
                    pos = n
                    break
            i = pos
            continue
        out.append(c)
        i += 1
    return ''.join(out)


def primary_command(cmd: str) -> str:
    """First non-env word. Skips leading FOO=bar prefixes."""
    for tok in cmd.lstrip().split():
        if re.match(r'^[A-Za-z_][A-Za-z0-9_]*=', tok):
            continue
        return tok.lstrip('(')
    return ''


def detect(cmd: str):
    issues = []
    masked = mask_strings_and_heredocs(cmd)

    # R1: && / ||
    if re.search(r'&&|\|\|', masked):
        issues.append(
            "R1: '&&' or '||' chains two commands. Split into separate Bash "
            "calls (parallel when independent)."
        )

    # R1: ;  (skip ';;' from case statements; require non-empty rhs)
    semi = re.search(r'(?<!;);(?!;)', masked)
    if semi and masked[semi.end():].strip():
        issues.append(
            "R1: ';' separates two commands. Split into separate Bash calls."
        )

    # R2: pipe-into-truncation
    if re.search(r'\|\s*(head|tail)\b', masked):
        issues.append(
            "R2: '| head' / '| tail' is output truncation. Drop the suffix; "
            "the full output is fine."
        )

    prim = primary_command(cmd)

    # R3: echo as primary
    if prim == 'echo':
        issues.append(
            "R3: 'echo' as primary is diagnostic-bash. The Bash tool result "
            "already reports exit code, stdout, stderr."
        )

    # R4: cat <file> for display (no heredoc anywhere in the command)
    if prim == 'cat' and '<<' not in masked:
        issues.append(
            "R4: 'cat <file>' is display-cat. Use Read(file) for display; "
            "heredoc form ('cat <<EOF ... EOF') stays allowed."
        )

    return issues


def main():
    try:
        data = json.load(sys.stdin)
    except Exception:
        sys.exit(0)
    if data.get('tool_name') != 'Bash':
        sys.exit(0)
    cmd = (data.get('tool_input') or {}).get('command') or ''
    if not cmd:
        sys.exit(0)
    issues = detect(cmd)
    if not issues:
        sys.exit(0)
    print("Blocked by bash-no-prompts policy:", file=sys.stderr)
    for s in issues:
        print("  - " + s, file=sys.stderr)
    print("", file=sys.stderr)
    print("Source: .claude/skills/bash-no-prompts/SKILL.md", file=sys.stderr)
    sys.exit(2)


if __name__ == '__main__':
    main()
