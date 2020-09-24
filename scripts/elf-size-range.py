#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation. All rights reserved.

# Please keep this file formatted with "black"

# This script is small because it does only one thing. Before extending
# it, considering dropping it and replacing it with one of its (much)
# bigger cousins. The Zephyr project has for instance
# scripts/footprint/size_report; there are others.

import argparse
import elftools
import configparser
import sys

from elftools.elf.elffile import ELFFile


def check_limits(sz, min, max):

    if max is not None and sz > max:
        print("FAIL: {} > {}".format(sz, max))
        return 1

    if min is not None and min > sz:
        print("FAIL: {} > {}".format(min, sz))
        return 1

    return 0


def check_section(e, reference, conf, section):

    failures = 0
    # can't use uncompressed sec.data_size yet with elftools version
    # 0.24 in Ubuntu 18
    sz = section["sh_size"]
    sname = section.name

    # Absolute limits
    min = conf[sname].getint("min")
    max = conf[sname].getint("max")

    if min is not None or max is not None:
        print(
            "Checking section {} size is within absolute limit(s): {} < {:d} < {}".format(
                sname, min, sz, max
            )
        )
        failures += check_limits(sz, min, max)

    # Relative limits
    if not reference:
        return failures

    dec = conf[sname].getint("max_decrease")
    inc = conf[sname].getint("max_increase")

    if dec is None and inc is None:
        return failures

    ref_section = reference.get_section_by_name(sname)
    ref_size = ref_section["sh_size"]
    print(
        (
            "Checking section {n} relative size compared to reference +/- deltas:"
            + (" {r:d} - {dec} <" if dec else "")
            + " {sz:d}"
            + (" < {r:d} + {inc}" if inc else "")
        ).format(n=sname, sz=sz, r=ref_size, dec=dec, inc=inc)
    )

    failures += check_limits(
        sz, ref_size - dec if dec else None, ref_size + inc if inc else None
    )

    return failures


def check_elf(e, reference, conf):

    failures = 0

    for sname in conf.sections():
        section = e.get_section_by_name(sname)
        if section is None:
            print("FAIL: Section '{}' not found in {}".format(sname, e.stream.name))
            failures += 1
            continue

        failures += check_section(e, reference, conf, section)

    return failures


def parse_args():

    parser = argparse.ArgumentParser(
        description="""Checks that ELF section sizes fall inside given limits.
Returns the number of violations."""
    )
    parser.add_argument(
        "-r", "--reference", help="reference ELF file to compare sizes against"
    )
    parser.add_argument(
        "-l",
        "--limits",
        help=""".conf file with size limits. Defaults to max_increase=100
which does nothing unless a --reference file is passed""",
    )
    parser.add_argument("elf_input", help="ELF input file")

    return parser.parse_args()


def main():
    args = parse_args()

    limits = configparser.ConfigParser()

    if args.limits:
        with open(args.limits) as f:
            limits.read_file(f)
    else:  # default configuration file
        for s in [".text", ".bss", ".data"]:
            limits[s] = {}
            limits[s]["max_increase"] = "100"

    e = ELFFile(open(args.elf_input, "rb"))
    ref = ELFFile(open(args.reference, "rb")) if args.reference else None

    sys.exit(check_elf(e, ref, limits))


if __name__ == "__main__":
    main()
