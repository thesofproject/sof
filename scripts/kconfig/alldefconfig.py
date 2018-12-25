#!/usr/bin/env python

# Copyright (c) 2018, Ulf Magnusson
# SPDX-License-Identifier: ISC

# Works like 'make alldefconfig'. Verified by the test suite to generate
# identical output to 'make alldefconfig' for all ARCHes.
#
# The default output filename is '.config'. A different filename can be passed
# in the KCONFIG_CONFIG environment variable.
#
# Usage for the Linux kernel:
#
#   $ make [ARCH=<arch>] scriptconfig SCRIPT=Kconfiglib/alldefconfig.py

import kconfiglib

def main():
    kconf = kconfiglib.standard_kconfig()
    kconfiglib.load_allconfig(kconf, "alldef.config")
    kconf.write_config()

if __name__ == "__main__":
    main()
