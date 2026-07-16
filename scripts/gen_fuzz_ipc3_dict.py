#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2026 Intel Corporation. All rights reserved.
#
# Generate dictionary/ipc3.dict for the SOF IPC3 libFuzzer harness.
#
# libFuzzer's dictionary is consulted by mutation strategies that
# inject known byte sequences into the candidate inputs.  For SOF
# IPC3 the single most valuable token is the 32-bit "cmd" dword that
# lives at offset 4 of every message (struct sof_ipc_cmd_hdr: the
# size dword is rewritten by the harness, so only cmd matters for
# dispatch).  That dword packs two fields:
#
#   glb_type : bits 28..31   (SOF_GLB_TYPE)
#   cmd_type : bits 16..27   (SOF_CMD_TYPE)
#
# The firmware switches on glb_type first and then on cmd_type, so a
# token that is only a bare glb_type reaches a handler but stalls at
# its inner switch.  We therefore emit, for every command group, the
# fully-combined "glb_type | cmd_type" dword that lands directly in a
# leaf handler, plus the bare glb_type values on their own (useful as
# a 4-byte splice when the fuzzer is building a message from scratch).
#
# The constants are harvested directly from src/include/ipc/header.h
# so the dictionary stays in sync with firmware code; only the
# SOF_GLB_TYPE() / SOF_CMD_TYPE() #defines are parsed, which keeps the
# harvester simple and robust.
#
# Usage:
#   python3 scripts/gen_fuzz_ipc3_dict.py [-o dictionary/ipc3.dict]
#
# The output is regenerated on demand from the header (for example by
# CI, into a temporary file) and is intentionally not committed to the
# tree; regenerate it whenever the harvested header changes.

import argparse
import re
import sys
from pathlib import Path

# Path to the IPC3 header, relative to the SOF repo root (the checkout
# that contains this script's `scripts/` directory).
HEADER_REL = Path("src/include/ipc/header.h")

# Bit layout of the command dword (see SOF_GLB_TYPE / SOF_CMD_TYPE in
# src/include/ipc/header.h).
GLB_TYPE_SHIFT = 28
CMD_TYPE_SHIFT = 16

# Command groups: each SOF_CMD_TYPE() value is only meaningful when
# OR-ed with the global type that selects its handler.  This table
# maps a cmd #define name prefix to the SOF_GLB_TYPE() #define that
# dispatches it.  Order controls the layout of the emitted file.
CMD_GROUPS = [
    ("SOF_IPC_TPLG_",   "SOF_IPC_GLB_TPLG_MSG"),
    ("SOF_IPC_PM_",     "SOF_IPC_GLB_PM_MSG"),
    ("SOF_IPC_COMP_",   "SOF_IPC_GLB_COMP_MSG"),
    ("SOF_IPC_DAI_",    "SOF_IPC_GLB_DAI_MSG"),
    ("SOF_IPC_STREAM_", "SOF_IPC_GLB_STREAM_MSG"),
    ("SOF_IPC_TRACE_",  "SOF_IPC_GLB_TRACE_MSG"),
    ("SOF_IPC_PROBE_",  "SOF_IPC_GLB_PROBE"),
    ("SOF_IPC_DEBUG_",  "SOF_IPC_GLB_DEBUG"),
    ("SOF_IPC_TEST_",   "SOF_IPC_GLB_TEST"),
]

GLB_RE = re.compile(
    r"#define\s+(?P<name>SOF_IPC_\w+)\s+SOF_GLB_TYPE\(\s*"
    r"(?P<value>0x[0-9a-fA-F]+|\d+)U?L?\s*\)"
)
CMD_RE = re.compile(
    r"#define\s+(?P<name>SOF_IPC_\w+)\s+SOF_CMD_TYPE\(\s*"
    r"(?P<value>0x[0-9a-fA-F]+|\d+)U?L?\s*\)"
)


def u32_le(value):
    """Encode an unsigned 32-bit value as 4 little-endian bytes."""
    value &= 0xFFFFFFFF
    return bytes([
        value & 0xFF,
        (value >> 8) & 0xFF,
        (value >> 16) & 0xFF,
        (value >> 24) & 0xFF,
    ])


def fmt_dict_entry(name, raw_bytes):
    """Format a libFuzzer dictionary line: name="\\xNN\\xNN...".

    libFuzzer accepts ASCII printables unescaped; we escape everything
    except space..~ (excluding the quote and backslash) which keeps the
    file readable while remaining unambiguous.
    """
    pieces = []
    for b in raw_bytes:
        if 0x20 <= b <= 0x7E and b not in (0x22, 0x5C):
            pieces.append(chr(b))
        else:
            pieces.append(f"\\x{b:02x}")
    return f'{name}="{"".join(pieces)}"'


def harvest(repo_root):
    """Parse header.h, returning (glb_defs, cmd_defs) name->value dicts."""
    path = repo_root / HEADER_REL
    if not path.is_file():
        print(f"error: {path} not found", file=sys.stderr)
        sys.exit(1)
    text = path.read_text()

    glb_defs = {}
    for m in GLB_RE.finditer(text):
        glb_defs[m.group("name")] = int(m.group("value"), 0) << GLB_TYPE_SHIFT

    cmd_defs = {}
    for m in CMD_RE.finditer(text):
        cmd_defs[m.group("name")] = int(m.group("value"), 0) << CMD_TYPE_SHIFT

    return glb_defs, cmd_defs


def build_sections(glb_defs, cmd_defs):
    """Return list of (section_label, [(name, raw_bytes), ...])."""
    sections = []

    # Bare global message types.
    glb_entries = [(name, u32_le(val)) for name, val in glb_defs.items()]
    if glb_entries:
        sections.append(("Global message types (glb_type only)", glb_entries))

    # Combined glb_type | cmd_type dwords, one section per group.
    for prefix, glb_name in CMD_GROUPS:
        glb_val = glb_defs.get(glb_name)
        if glb_val is None:
            print(f"warning: {glb_name} not found in header, "
                  f"skipping group {prefix}*", file=sys.stderr)
            continue
        entries = []
        for name, cmd_val in cmd_defs.items():
            if not name.startswith(prefix):
                continue
            entries.append((name, u32_le(glb_val | cmd_val)))
        if entries:
            sections.append((f"{glb_name} commands", entries))

    return sections


def render(sections):
    out_lines = [
        "# SOF IPC3 libFuzzer dictionary",
        "#",
        "# Generated by scripts/gen_fuzz_ipc3_dict.py from the IPC3",
        "# header src/include/ipc/header.h.  Do not edit by hand;",
        "# regenerate instead.",
        "#",
        "# Each entry is a 4-byte little-endian command dword.  Command",
        "# entries combine the global type and the command type so they",
        "# land directly in a leaf handler when libFuzzer's CMP /",
        "# dictionary mutators splice them into the cmd field at offset 4",
        "# of a message.",
        "",
    ]
    for label, entries in sections:
        out_lines.append(f"# --- {label} ---")
        for name, raw in entries:
            out_lines.append(fmt_dict_entry(name, raw))
        out_lines.append("")
    return "\n".join(out_lines)


def main():
    parser = argparse.ArgumentParser(
        description="Generate the SOF IPC3 libFuzzer dictionary from the "
                    "in-tree IPC3 header.")
    parser.add_argument(
        "-o", "--output",
        help="Output path (default: stdout)",
    )
    parser.add_argument(
        "--repo-root",
        default=None,
        help="SOF repository root (default: auto-detected from script "
             "location)",
    )
    args = parser.parse_args()

    if args.repo_root:
        repo_root = Path(args.repo_root).resolve()
    else:
        # Script lives at <repo>/scripts/gen_fuzz_ipc3_dict.py.
        repo_root = Path(__file__).resolve().parents[1]

    glb_defs, cmd_defs = harvest(repo_root)
    sections = build_sections(glb_defs, cmd_defs)
    if not sections:
        print("error: no dictionary entries harvested; refusing to write "
              "an empty dictionary", file=sys.stderr)
        sys.exit(1)
    text = render(sections)
    if args.output:
        Path(args.output).write_text(text)
    else:
        sys.stdout.write(text)


if __name__ == "__main__":
    main()
