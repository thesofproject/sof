#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2026 Intel Corporation. All rights reserved.

"""Cross-check topology2 token definitions against the kernel schema.

Reports, using schema/kernel_tokens.py:
  - tokens the IPC4 kernel driver reads that tokens.conf does not define
  - tokens.conf entries unknown to the kernel (informational)
  - token_ref attributes in topology2 classes naming undefined token sets

Exit status is 1 for missing kernel tokens or undefined token sets.
"""

import argparse
import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

# pylint: disable=wrong-import-position
from schema import kernel_tokens as kt  # noqa: E402

TOPOLOGY2 = pathlib.Path(__file__).resolve().parents[2] / "topology2"


def parse_tokens_conf(path: pathlib.Path) -> dict:
    "set name -> {token name: id}"
    sets, current = {}, None
    for line in path.read_text().splitlines():
        line = line.split("#", 1)[0].strip()
        m = re.match(r'name\s+"(\w+)"', line)
        if m:
            current = sets.setdefault(m.group(1), {})
            continue
        m = re.match(r"(\w+)\s+(\d+)$", line)
        if m and current is not None:
            current[m.group(1)] = int(m.group(2))
    return sets


def kernel_ipc4_tokens() -> set:
    tokens = {tok for _desc, table in kt.IPC4_TOKEN_LIST.values() for tok, _t, _g in table}
    for table in kt.GENERIC_TABLES.values():
        tokens |= {tok for tok, _t, _g in table}
    return tokens


def token_refs(root: pathlib.Path):
    for conf in sorted(root.rglob("*.conf")):
        for lineno, line in enumerate(conf.read_text(errors="replace").splitlines(), 1):
            m = re.search(r'token_ref\s+"(\w+)\.(\w+)"', line.split("#", 1)[0])
            if m:
                yield conf, lineno, m.group(1), m.group(2)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--topology2", type=pathlib.Path, default=TOPOLOGY2)
    ap.add_argument("-v", "--verbose", action="store_true", help="also list unknown tokens")
    args = ap.parse_args()

    sets = parse_tokens_conf(args.topology2 / "include/common/tokens.conf")
    defined = {tid for s in sets.values() for tid in s.values()}
    failed = False

    for tok in sorted(kernel_ipc4_tokens() - defined):
        print(f"error: kernel reads {kt.TOKEN_NAMES.get(tok, tok)} ({tok}), "
              f"tokens.conf does not define it")
        failed = True

    unknown = sorted(defined - set(kt.TOKEN_NAMES))
    if unknown and args.verbose:
        print(f"info: tokens.conf IDs unknown to the kernel: {', '.join(map(str, unknown))}")

    for conf, lineno, set_name, ttype in token_refs(args.topology2):
        if set_name not in sets:
            print(f"error: {conf.relative_to(args.topology2)}:{lineno}: token_ref "
                  f"'{set_name}.{ttype}' names an undefined token set")
            failed = True

    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
