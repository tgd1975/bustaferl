#!/usr/bin/env python3
"""Render `make help` in a two-column layout grouped by Makefile sections.

Sections are detected from `# --- <name> ---` comment lines and targets
from the standard `name: ... ## description` convention. Sections are
split between left/right columns at the boundary that produces the most
balanced column heights, preserving Makefile order within each column.
"""
import re
import sys
from pathlib import Path

SECTION_RE = re.compile(r"^# --- (.+?) ---\s*$")
TARGET_RE = re.compile(r"^([a-zA-Z0-9_-]+):.*?## (.+?)\s*$")
ANSI_RE = re.compile(r"\033\[[0-9;]*m")

USE_ANSI = sys.stdout.isatty()
BOLD = "\033[1m" if USE_ANSI else ""
RESET = "\033[0m" if USE_ANSI else ""
TARGET_WIDTH = 30
GUTTER = 2


def parse(path):
    preamble = []
    sections = []
    current = None
    for line in path.read_text().splitlines():
        m = SECTION_RE.match(line)
        if m:
            current = (m.group(1), [])
            sections.append(current)
            continue
        m = TARGET_RE.match(line)
        if m:
            entry = (m.group(1), m.group(2))
            if current is None:
                preamble.append(entry)
            else:
                current[1].append(entry)
    return preamble, sections


def section_lines(name, entries):
    lines = [f"{BOLD}{name}{RESET}"]
    for tgt, desc in entries:
        lines.append(f"  {tgt:<{TARGET_WIDTH}} {desc}")
    return lines


def visible_len(s):
    return len(ANSI_RE.sub("", s))


def pad(s, w):
    return s + " " * max(0, w - visible_len(s))


def split_balanced(sections):
    # Greedy split at the boundary nearest the midpoint; on ties, prefer the
    # later cut so the left column ends up at least as long as the right.
    heights = [1 + len(entries) + 1 for _, entries in sections]
    total = sum(heights)
    half = total / 2
    best_idx, best_diff = 0, total
    cum = 0
    for i, h in enumerate(heights):
        cum += h
        diff = abs(cum - half)
        if diff <= best_diff:
            best_diff, best_idx = diff, i
    return sections[: best_idx + 1], sections[best_idx + 1 :]


def render(preamble, sections):
    out = []
    for tgt, desc in preamble:
        out.append(f"  {tgt:<{TARGET_WIDTH}} {desc}")
    left_secs, right_secs = split_balanced(sections)
    left_lines, right_lines = [], []
    for name, entries in left_secs:
        left_lines.append("")
        left_lines.extend(section_lines(name, entries))
    for name, entries in right_secs:
        right_lines.append("")
        right_lines.extend(section_lines(name, entries))
    col_width = max((visible_len(s) for s in left_lines), default=0)
    n = max(len(left_lines), len(right_lines))
    for i in range(n):
        l = left_lines[i] if i < len(left_lines) else ""
        r = right_lines[i] if i < len(right_lines) else ""
        out.append(f"{pad(l, col_width)}{' ' * GUTTER}{r}".rstrip())
    return "\n".join(out)


def main():
    paths = [Path(p) for p in sys.argv[1:]] or [Path("Makefile")]
    preamble, sections = [], []
    for p in paths:
        pre, secs = parse(p)
        preamble.extend(pre)
        sections.extend(secs)
    print(render(preamble, sections))


if __name__ == "__main__":
    main()
