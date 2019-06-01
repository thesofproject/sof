#!/usr/bin/env python

# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright (c) 2019, Intel Corporation. All rights reserved.
#
# Author: Janusz Jankowski <janusz.jankowski@linux.intel.com>

# Loads current config file and saves minimal configuration to
# the given file.
#
# Usage:
#   savedefconfig.py KCONFIG_FILENAME DEFCONFIG_OUTPUT_FILE

import os
import sys

import kconfiglib


def main():
    if len(sys.argv) < 3:
        sys.exit("usage: {} KCONFIG_FILENAME DEFCONFIG_OUTPUT_FILE"
            .format(sys.argv[0]))

    kconf = kconfiglib.Kconfig(sys.argv[1])
    kconf.load_config()
    kconf.write_min_config(sys.argv[2])


if __name__ == "__main__":
    main()
