#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2026 Intel Corporation.

"""Add missing '/* CONDITION */' comments to preprocessor #endif (and
optionally #else) lines that are far away from their matching #if /
#ifdef / #ifndef.

SOF style tags long conditional blocks like this:

    #if FOO
    ...
    #endif /* FOO */

This script finds #endif lines without such a trailing comment whose
matching #if/#ifdef/#ifndef is more than --threshold lines away, and
appends the comment. Nested conditionals are handled recursively.

Usage:
    scripts/add-endif-comments.py [--threshold N] [--else] [--apply] PATH...

By default the script only prints a unified diff of what it would
change (dry run). Pass --apply to modify files in place.
"""

import argparse
import difflib
import re
import sys
from pathlib import Path

DEFAULT_THRESHOLD = 15
DEFAULT_EXTENSIONS = {".c", ".h", ".cpp", ".hpp", ".cc", ".hh"}
SKIP_DIRS = {".git"}

IF_RE = re.compile(r'^\s*#\s*(if|ifdef|ifndef)\b(.*)$')
ELIF_RE = re.compile(r'^\s*#\s*elif\b')
ELSE_RE = re.compile(r'^\s*#\s*else\b')
ENDIF_RE = re.compile(r'^\s*#\s*endif\b')
COMMENT_RE = re.compile(r'/\*|//')


def compute_comment_state(lines):
    """Return a list, parallel to lines, telling whether each line starts
    inside an unterminated /* */ block comment. Used so that #if-like text
    inside a comment is never mistaken for a real directive."""
    states = []
    in_block_comment = False
    for line in lines:
        states.append(in_block_comment)
        i, length = 0, len(line)
        in_string = in_char = False
        while i < length:
            if in_block_comment:
                end = line.find('*/', i)
                if end == -1:
                    break
                in_block_comment = False
                i = end + 2
                continue
            c = line[i]
            if in_string:
                i += 2 if c == '\\' else 1
                if c == '"':
                    in_string = False
                continue
            if in_char:
                i += 2 if c == '\\' else 1
                if c == "'":
                    in_char = False
                continue
            if c == '"':
                in_string = True
            elif c == "'":
                in_char = True
            elif c == '/' and i + 1 < length:
                nxt = line[i + 1]
                if nxt == '*':
                    in_block_comment = True
                    i += 2
                    continue
                if nxt == '/':
                    break
            i += 1
    return states


def format_condition(text):
    """Normalize a raw #if/#ifdef/#ifndef argument into comment text."""
    text = re.sub(r'/\*.*?\*/', '', text)
    text = re.sub(r'//.*$', '', text)
    return re.sub(r'\s+', ' ', text).strip()


def join_continuation(lines, i):
    """Join a directive line with any backslash-continued lines that follow.
    Returns (joined_text, index_of_last_physical_line)."""
    parts = [lines[i].rstrip('\n')]
    j = i
    while parts[-1].rstrip().endswith('\\') and j + 1 < len(lines):
        j += 1
        parts.append(lines[j].rstrip('\n'))
    joined = ' '.join(p.rstrip().rstrip('\\').strip() for p in parts)
    return joined, j


def has_trailing_comment(line):
    return bool(COMMENT_RE.search(line))


def append_comment(line, condition):
    return f"{line.rstrip(chr(10))} /* {condition} */\n"


def process_block(lines, start_idx, comment_state, threshold, do_else, changes):
    """Handle the #if/#ifdef/#ifndef block that opens at start_idx.
    Recurses into any nested conditional found along the way.
    Returns the index of the line right after the matching #endif."""
    match = IF_RE.match(lines[start_idx])
    joined, directive_end = join_continuation(lines, start_idx)
    text_match = re.match(r'^\s*#\s*(?:if|ifdef|ifndef)\b(.*)$', joined)
    condition = format_condition(text_match.group(1) if text_match else match.group(2))

    i = directive_end + 1
    n = len(lines)
    while i < n:
        if comment_state[i]:
            i += 1
            continue
        line = lines[i]
        if IF_RE.match(line):
            i = process_block(lines, i, comment_state, threshold, do_else, changes)
            continue
        if ENDIF_RE.match(line):
            distance = i - start_idx
            if distance > threshold and not has_trailing_comment(line):
                lines[i] = append_comment(line, condition)
                changes.append((i + 1, condition))
            return i + 1
        if do_else and ELSE_RE.match(line):
            distance = i - start_idx
            if distance > threshold and not has_trailing_comment(line):
                lines[i] = append_comment(line, condition)
                changes.append((i + 1, condition))
        i += 1
    # Reached EOF without a matching #endif (malformed file, or macro
    # trickery); nothing more we can do for this block.
    return i


def process_lines(lines, threshold, do_else):
    """Mutates lines in place, returns list of (line_no, condition) changes."""
    comment_state = compute_comment_state(lines)
    changes = []
    i, n = 0, len(lines)
    while i < n:
        if not comment_state[i] and IF_RE.match(lines[i]):
            i = process_block(lines, i, comment_state, threshold, do_else, changes)
        else:
            i += 1
    return changes


def iter_source_files(paths, extensions):
    for path in paths:
        p = Path(path)
        if p.is_dir():
            for sub in sorted(p.rglob('*')):
                if any(part in SKIP_DIRS for part in sub.parts):
                    continue
                if sub.is_file() and sub.suffix in extensions:
                    yield sub
        elif p.is_file():
            yield p
        else:
            print(f"warning: {p} not found", file=sys.stderr)


def process_file(path, threshold, do_else, apply_changes):
    original_text = path.read_text(encoding='utf-8', errors='surrogateescape')
    lines = original_text.splitlines(keepends=True)
    if lines and not lines[-1].endswith('\n'):
        lines[-1] += '\n'
        trailing_newline_added = True
    else:
        trailing_newline_added = False

    changes = process_lines(lines, threshold, do_else)
    if not changes:
        return changes

    new_text = ''.join(lines)
    if trailing_newline_added:
        new_text = new_text[:-1]

    if apply_changes:
        path.write_text(new_text, encoding='utf-8', errors='surrogateescape')
    else:
        diff = difflib.unified_diff(
            original_text.splitlines(keepends=True),
            new_text.splitlines(keepends=True),
            fromfile=str(path), tofile=str(path),
        )
        sys.stdout.writelines(diff)
    return changes


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                      formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('paths', nargs='+', help='files or directories to scan')
    parser.add_argument('--threshold', type=int, default=DEFAULT_THRESHOLD,
                         help=f'minimum block length to require a comment (default: {DEFAULT_THRESHOLD})')
    parser.add_argument('--else', dest='do_else', action='store_true',
                         help='also add comments to far-away #else lines')
    parser.add_argument('--apply', action='store_true',
                         help='write changes to disk instead of printing a diff')
    parser.add_argument('--ext', action='append', dest='extensions',
                         help='additional file extension to scan (e.g. --ext .cc), repeatable')
    args = parser.parse_args()

    extensions = set(DEFAULT_EXTENSIONS)
    if args.extensions:
        extensions.update(e if e.startswith('.') else f'.{e}' for e in args.extensions)

    total_changes = 0
    total_files = 0
    for path in iter_source_files(args.paths, extensions):
        changes = process_file(path, args.threshold, args.do_else, args.apply)
        if changes:
            total_files += 1
            total_changes += len(changes)
            action = "updated" if args.apply else "would update"
            print(f"# {action} {path}: {len(changes)} comment(s)", file=sys.stderr)

    print(f"# total: {total_changes} comment(s) in {total_files} file(s)", file=sys.stderr)
    return 0


if __name__ == '__main__':
    sys.exit(main())
