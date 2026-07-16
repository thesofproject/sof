#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2026 Intel Corporation. All rights reserved.
#
# Generate dictionary/ipc4.dict for the SOF IPC4 libFuzzer harness.
#
# libFuzzer's dictionary is consulted by mutation strategies that
# inject known byte sequences into the candidate inputs.  For SOF
# IPC4 the most valuable seed values are:
#
#   * The 4-byte "primary" message header dat for each well-known
#     dispatch path: msg_tgt|rsp|type encoded into bits 24-30 with
#     the rest left zero so libFuzzer can mutate the lower 24 bits
#     (which carry module_id / instance_id / global type parameters).
#   * Small enum constants that appear inline in payloads
#     (pipeline state values, pipeline extension object IDs,
#     large_config param IDs).
#
# The constants are harvested directly from sof/src/include/ipc4/*.h
# so the dictionary stays in sync with firmware code; only enums
# whose entries are all integer literals (decimal or hex) are
# parsed, which keeps the harvester simple and robust.
#
# Usage:
#   python3 scripts/gen_fuzz_ipc4_dict.py [-o dictionary/ipc4.dict]
#
# The output is regenerated on demand from the headers (for example by
# CI, into a temporary file) and is intentionally not committed to the
# tree; regenerate it whenever the harvested headers change.

import argparse
import re
import sys
from pathlib import Path

# Path to the IPC4 public include directory, relative to the SOF repo
# root (the checkout that contains this script's `scripts/` directory).
IPC4_INC_REL = Path("src/include/ipc4")

# Enums we want to harvest, keyed by the source header (relative to
# IPC4_INC_REL) and the C enum tag.  Each entry also records what
# kind of dictionary token to emit:
#   "global_pri"  - 4-byte LE pri value with msg_tgt=0, rsp=0, type=N
#   "module_pri"  - 4-byte LE pri value with msg_tgt=1, rsp=0, type=N
#   "u32"         - raw 4-byte LE value of N
ENUM_SOURCES = [
    ("header.h",        "ipc4_message_type",          "global_pri"),
    ("module.h",        "sof_ipc4_module_type",       "module_pri"),
    ("module.h",        "ipc4_mod_init_data_glb_id",  "u32"),
    ("pipeline.h",      "ipc4_pipeline_state",        "u32"),
    ("pipeline.h",      "ipc4_pipeline_ext_obj_id",   "u32"),
    ("pipeline.h",      "ipc4_pipeline_priority",     "u32"),
    ("base_fw.h",       "ipc4_basefw_params",         "u32"),
    ("base_fw.h",       "ipc4_fw_config_params",      "u32"),
    ("base_fw.h",       "ipc4_hw_config_params",      "u32"),
    ("base_fw.h",       "ipc4_memory_type",           "u32"),
    ("base_fw.h",       "ipc4_clock_src",             "u32"),
    ("notification.h",  "sof_ipc4_notification_type", "u32"),
    ("notification.h",  "sof_ipc4_resource_event_type", "u32"),
    ("notification.h",  "sof_ipc4_resource_type",     "u32"),
    ("error_status.h",  "ipc4_status",                "u32"),
]

# Bit layout of struct ipc4_message_request::primary (see
# src/include/ipc4/header.h):
#   rsvd0   : 24   bits 0..23
#   type    :  5   bits 24..28
#   rsp     :  1   bit  29
#   msg_tgt :  1   bit  30
#   reserved:  1   bit  31
PRI_TYPE_SHIFT     = 24
PRI_RSP_SHIFT      = 29
PRI_MSG_TGT_SHIFT  = 30

ENUM_BLOCK_RE = re.compile(
    r"enum\s+(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*\{(?P<body>.*?)\}\s*;",
    re.DOTALL,
)

# Match `NAME` or `NAME = VALUE` inside an enum body.  VALUE may be
# decimal or hex.  Any non-numeric initialiser causes us to skip the
# whole enum (we don't resolve macros / arithmetic).
ENUM_ENTRY_RE = re.compile(
    r"""
    ^\s*
    (?P<name>[A-Za-z_][A-Za-z0-9_]*)
    (?:\s*=\s*(?P<value>0x[0-9a-fA-F]+|\d+))?
    \s*,?\s*(?://.*|/\*.*?\*/)?\s*$
    """,
    re.VERBOSE,
)


def find_enum(header_text, enum_tag):
    """Return the enum body text for `enum enum_tag { ... }`, else None."""
    for m in ENUM_BLOCK_RE.finditer(header_text):
        if m.group("name") == enum_tag:
            return m.group("body")
    return None


def parse_enum(body):
    """Parse a C enum body of integer-literal entries.

    Returns a list of (name, value) pairs.  If an entry uses a
    non-literal initialiser (macro, arithmetic, cross-reference to
    another enum name) we stop parsing at that point and return
    everything harvested so far -- typically the trailing sentinels
    (``_MAX``, ``_COUNT``) of an otherwise integer-literal enum.
    Returning a truncated list is preferable to silently emitting
    wrong values for auto-incremented entries that follow."""
    out = []
    next_value = 0
    body = re.sub(r"/\*.*?\*/", "", body, flags=re.DOTALL)
    body = re.sub(r"//[^\n]*", "", body)
    for raw_line in body.splitlines():
        line = raw_line.strip()
        if not line:
            continue
        m = ENUM_ENTRY_RE.match(line)
        if not m:
            # First non-literal entry: stop here.  Anything that
            # follows would need correct auto-increment tracking
            # which we cannot guarantee once the count is broken.
            break
        name = m.group("name")
        if m.group("value") is not None:
            value = int(m.group("value"), 0)
        else:
            value = next_value
        out.append((name, value))
        next_value = value + 1
    return out


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

    libFuzzer accepts ASCII printables unescaped; we escape
    everything except space..~ excluding the quote and backslash,
    which keeps the file readable while remaining unambiguous.
    """
    pieces = []
    for b in raw_bytes:
        if 0x20 <= b <= 0x7E and b not in (0x22, 0x5C):
            pieces.append(chr(b))
        else:
            pieces.append(f"\\x{b:02x}")
    return f'{name}="{"".join(pieces)}"'


def encode(kind, value):
    if kind == "global_pri":
        return u32_le(value << PRI_TYPE_SHIFT)
    if kind == "module_pri":
        return u32_le((1 << PRI_MSG_TGT_SHIFT) | (value << PRI_TYPE_SHIFT))
    if kind == "u32":
        return u32_le(value)
    raise ValueError(f"unknown kind: {kind}")


def harvest(repo_root):
    """Return list of (section_label, [(name, raw_bytes), ...])."""
    sections = []
    for fname, enum_tag, kind in ENUM_SOURCES:
        path = repo_root / IPC4_INC_REL / fname
        if not path.is_file():
            print(f"error: {path} not found", file=sys.stderr)
            sys.exit(1)
        text = path.read_text()
        body = find_enum(text, enum_tag)
        if body is None:
            print(f"warning: enum {enum_tag} not found in {fname}",
                  file=sys.stderr)
            continue
        entries = parse_enum(body)
        if not entries:
            print(f"warning: enum {enum_tag} in {fname} yielded no "
                  "literal entries, skipping", file=sys.stderr)
            continue
        encoded = []
        for name, value in entries:
            # Skip MAX / COUNT sentinels which are not real dispatch
            # values; keep everything else even if the value collides
            # with another entry (libFuzzer dedups automatically).
            if (name.endswith("_MAX")
                    or name.endswith("_MAX_IXC_MESSAGE_TYPE")
                    or name.endswith("_PARAMS_COUNT")):
                continue
            # The primary-header `type` field is only 5 bits wide; a
            # value that does not fit cannot be a real dispatch type and
            # would corrupt the rsp / msg_tgt bits, so drop it regardless
            # of the enumerator's name (robust to header typo fixes).
            if kind in ("global_pri", "module_pri") and not 0 <= value < 32:
                continue
            encoded.append((name, encode(kind, value)))
        sections.append((f"{fname}::{enum_tag} ({kind})", encoded))
    return sections


def render(sections):
    out_lines = [
        "# SOF IPC4 libFuzzer dictionary",
        "#",
        "# Generated by scripts/gen_fuzz_ipc4_dict.py from the IPC4",
        "# public headers under src/include/ipc4/.  Do not edit by",
        "# hand; regenerate instead.",
        "#",
        "# Each entry is a 4-byte little-endian value drawn from an IPC4",
        "# enum, encoded so libFuzzer's CMP / dictionary mutators can",
        "# splice it into candidate inputs at any 4-byte-aligned offset.",
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
        description="Generate the SOF IPC4 libFuzzer dictionary from the "
                    "in-tree IPC4 headers.")
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
        # Script lives at <repo>/scripts/gen_fuzz_ipc4_dict.py.
        repo_root = Path(__file__).resolve().parents[1]

    sections = harvest(repo_root)
    if not any(entries for _, entries in sections):
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
