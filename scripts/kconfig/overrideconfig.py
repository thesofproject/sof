#!/usr/bin/env python

# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright (c) 2019, Intel Corporation. All rights reserved.
#
# Author: Janusz Jankowski <janusz.jankowski@linux.intel.com>

# Loads current config file and updates with values from another
# config file. Configs that are not set in neither of files are set
# to default value.
#
# Usage:
#   overrideconfig.py KCONFIG_FILENAME CONFIG_OVERRIDE_FILE

import os
import sys

import kconfiglib


def main():
    if len(sys.argv) < 3:
        sys.exit("usage: {} KCONFIG_FILENAME CONFIG_OVERRIDE_FILE"
	    .format(sys.argv[0]))

    kconf = kconfiglib.Kconfig(sys.argv[1])
    kconf.load_config()

    kconf.disable_override_warnings()
    kconf.disable_redun_warnings()

    kconf.load_config(filename=sys.argv[2], replace=False)

    kconf.enable_override_warnings()
    kconf.enable_redun_warnings()

    kconf.write_config()


if __name__ == "__main__":
    main()
