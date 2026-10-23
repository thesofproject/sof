#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2026 Intel Corporation. All rights reserved.

"""Offline validator for SOF IPC4 topology binaries.

Parses .tplg files and repeats the checks the Linux SOF driver does when
it loads a topology, without needing the target hardware. Exit status is
1 if any error (or, with --werror, warning) was found.

    tplg_validate.py sof-mtl-nocodec.tplg
    tplg_validate.py --werror --num-cores 5 build/*.tplg
    tplg_validate.py --list-rules
"""

import argparse
import fnmatch
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

# pylint: disable=wrong-import-position
import checks  # noqa: E402
from checks import ERROR, INFO, WARNING  # noqa: E402
from schema import kernel_tokens  # noqa: E402
from tplgparse import model  # noqa: E402


def load_waivers(path) -> list:
    """Waiver file lines: '<topology name glob> <rule id> [reason]'.

    Waived errors are reported as warnings so that known problems do not
    fail the build while new ones do.
    """
    waivers = []
    with open(path, encoding="utf-8") as f:
        for line in f:
            fields = line.split("#", 1)[0].split()
            if len(fields) >= 2:
                waivers.append((fields[0], fields[1]))
    return waivers


def apply_waivers(path, findings, waivers):
    name = os.path.basename(path)
    if name.endswith(".tplg"):
        name = name[:-len(".tplg")]
    for f in findings:
        if f.severity == ERROR and any(fnmatch.fnmatchcase(name, pat) and f.rule == rule
                                       for pat, rule in waivers):
            f.severity = WARNING
            f.message += " [waived]"


def list_rules():
    for r in checks.load_rules():
        print(f"{r.id}  {r.layer}  {r.severity:7}  {r.summary}")
        print(f"                      kernel: {r.kernel}")
    print(f"\ntoken schema generated from kernel {kernel_tokens.KERNEL_VERSION}")


def dump(tplg):
    "Print the decoded topology with kernel token names."
    names = kernel_tokens.TOKEN_NAMES
    for w in tplg.widgets:
        print(f"widget {w.name!r} type {w.type_name} pipeline {w.index} sname {w.sname!r}")
        for a in w.priv.arrays:
            for t in a.tuples:
                value = model.format_uuid(t.value) if t.type.name == "UUID" else t.value
                print(f"    {names.get(t.token, t.token)} = {value!r}")
        for k in w.kcontrols:
            print(f"    {k.label} ({k.kind.name})")
    for r in tplg.routes:
        print(r.label)
    for p in tplg.pcms:
        print(f"{p.label} playback {p.raw.playback} capture {p.raw.capture}")
    for link in tplg.links:
        print(f"{link.label} stream {link.stream_name!r} hw_configs {link.raw.num_hw_configs}")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("files", nargs="*", help="topology binaries")
    ap.add_argument("--werror", action="store_true", help="treat warnings as errors")
    ap.add_argument("-q", "--quiet", action="store_true",
                    help="print errors only, plus a one line summary of warnings")
    ap.add_argument("-v", "--verbose", action="store_true", help="also print info findings")
    ap.add_argument("--num-cores", type=int, default=0,
                    help="DSP core count for core ID checks (default: not checked)")
    ap.add_argument("--rules", help="comma separated rule IDs or layers (L1..L4) to run")
    ap.add_argument("--disable", default="", help="comma separated rule IDs to skip")
    ap.add_argument("--waivers", help="file of known errors to report as warnings")
    ap.add_argument("--json", action="store_true", help="machine readable output")
    ap.add_argument("--dump", action="store_true", help="print the decoded topology")
    ap.add_argument("--list-rules", action="store_true", help="list rules and exit")
    ap.add_argument("--remove-on-error", action="store_true",
                    help="delete a topology file that fails validation, so that a build "
                         "system rebuilds and rechecks it next time")
    args = ap.parse_args()

    if args.list_rules:
        list_rules()
        return 0
    if not args.files:
        ap.error("no topology files given")

    all_rules = checks.load_rules()
    wanted = set(args.rules.split(",")) if args.rules else None
    disabled = set(filter(None, args.disable.split(",")))
    selected = {r.id for r in all_rules
                if (wanted is None or r.id in wanted or r.layer in wanted)
                and r.id not in disabled}

    options = checks.Options(num_cores=args.num_cores, verbose=args.verbose)
    waivers = load_waivers(args.waivers) if args.waivers else []
    failed = False
    report = {}

    for path in args.files:
        try:
            tplg = model.load(path)
        except OSError as e:
            print(f"{path}: {e}", file=sys.stderr)
            failed = True
            continue
        if args.dump:
            dump(tplg)
        findings = checks.run(tplg, options, selected)
        apply_waivers(path, findings, waivers)
        counts = {s: sum(1 for f in findings if f.severity == s) for s in (ERROR, WARNING, INFO)}
        if counts[ERROR] or (args.werror and counts[WARNING]):
            failed = True
            if args.remove_on_error:
                os.remove(path)

        if args.json:
            report[path] = [f.__dict__ for f in findings]
            continue
        shown = {ERROR} if args.quiet else {ERROR, WARNING}
        if args.verbose:
            shown.add(INFO)
        for f in findings:
            if f.severity in shown or (args.werror and f.severity == WARNING):
                print(f"{path}: {f.format()}")
        if not args.quiet or counts[ERROR] or counts[WARNING]:
            print(f"{path}: {counts[ERROR]} errors, {counts[WARNING]} warnings, "
                  f"{counts[INFO]} info")

    if args.json:
        json.dump(report, sys.stdout, indent=1)
        print()
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
