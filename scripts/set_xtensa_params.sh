# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 Intel Corporation. All rights reserved.

# Because this is meant to be sourced
# shellcheck disable=SC2148
# shellcheck disable=SC2034

### XTENSA_ toolchain configuration shared across projects ###

# These variables are currently used in/by:
#
# - xtensa-build-all.sh (XTOS)
# - script/rebuild-testbench.sh
# - before Zephyr's `twister` or `west build`
#
# Not all variables are used in all use cases. Some are.
#
# Find some information about XTENSA_SYSTEM and XTENSA_CORE in
# ./scripts/xtensa-build-zephyr.py --help
#
# The variables used by Zephyr are duplicated in
# xtensa-build-zephyr.py, please keep in sync!

# To maximize reuse, keep this script very basic and minimal and don't
# `export`: leave which the decision for each variable to the caller.


# Sourced script argument $1 is a non-standard bash extension
[ -n "$1" ] || {
    >&2 printf 'Missing platform argument\n'
    return 1 # Not exit!
}
platform=$1

# If adding a new variable is required, avoid adding it to the mass
# duplication in this first, very verbose `case` statement. Prefer
# adding a new, smarter, per-variable `case` statement like the one for
# ZEPHYR_TOOLCHAIN_VARIANT below
PLATFORM="$platform"
case "$platform" in

    # Intel
    tgl)
	PLATFORM="tgplp"
	XTENSA_CORE="cavs2x_LX6HiFi3_2017_8"
	HOST="xtensa-cnl-elf"
	TOOLCHAIN_VER="RG-2017.8-linux"
	HAVE_ROM='yes'
	IPC4_CONFIG_OVERLAY="tigerlake_ipc4"
	# default key for TGL
	PLATFORM_PRIVATE_KEY="-D${SIGNING_TOOL}_PRIVATE_KEY=$SOF_TOP/keys/otc_private_key_3k.pem"
	;;
    tgl-h)
	PLATFORM="tgph"
	XTENSA_CORE="cavs2x_LX6HiFi3_2017_8"
	HOST="xtensa-cnl-elf"
	TOOLCHAIN_VER="RG-2017.8-linux"
	HAVE_ROM='yes'
	# default key for TGL
	PLATFORM_PRIVATE_KEY="-D${SIGNING_TOOL}_PRIVATE_KEY=$SOF_TOP/keys/otc_private_key_3k.pem"
	;;
    mtl|lnl)
	XTENSA_CORE="ace10_LX7HiFi4_2022_10"
	TOOLCHAIN_VER="RI-2022.10-linux"
	;;

    # NXP
    imx8)
	XTENSA_CORE="hifi4_nxp_v3_3_1_2_2017"
	HOST="xtensa-imx-elf"
	TOOLCHAIN_VER="RG-2017.8-linux"
	;;
    imx8x)
	XTENSA_CORE="hifi4_nxp_v3_3_1_2_2017"
	HOST="xtensa-imx-elf"
	TOOLCHAIN_VER="RG-2017.8-linux"
	;;
    imx8m)
	XTENSA_CORE="hifi4_mscale_v0_0_2_2017"
	HOST="xtensa-imx8m-elf"
	TOOLCHAIN_VER="RG-2017.8-linux"
	;;
    imx8ulp)
	XTENSA_CORE="hifi4_nxp2_ulp_prod"
	HOST="xtensa-imx8ulp-elf"
	TOOLCHAIN_VER="RG-2017.8-linux"
	;;

    # AMD
    rn)
	PLATFORM="renoir"
	XTENSA_CORE="ACP_3_1_001_PROD_2019_1"
	HOST="xtensa-rn-elf"
	TOOLCHAIN_VER="RI-2019.1-linux"
	;;
    rmb)
	PLATFORM="rembrandt"
	ARCH="xtensa"
	XTENSA_CORE="LX7_HiFi5_PROD"
	HOST="xtensa-rmb-elf"
	TOOLCHAIN_VER="RI-2019.1-linux"
	;;
    vangogh)
	ARCH="xtensa"
	XTENSA_CORE="ACP_5_0_001_PROD"
	HOST="xtensa-vangogh-elf"
	TOOLCHAIN_VER="RI-2019.1-linux"
	;;
    acp_6_3)
	ARCH="xtensa"
	XTENSA_CORE="ACP_6_3_HiFi5_PROD_Linux"
	HOST="xtensa-acp_6_3-elf"
	TOOLCHAIN_VER="RI-2021.6-linux"
	;;

    # Mediatek
    mt8186)
	XTENSA_CORE="hifi5_7stg_I64D128"
	HOST="xtensa-mt8186-elf"
	TOOLCHAIN_VER="RI-2020.5-linux"
	;;
    mt8188)
	XTENSA_CORE="hifi5_7stg_I64D128"
	HOST="xtensa-mt8188-elf"
	TOOLCHAIN_VER="RI-2020.5-linux"
	;;
    mt8195)
	XTENSA_CORE="hifi4_8195_PROD"
	HOST="xtensa-mt8195-elf"
	TOOLCHAIN_VER="RI-2019.1-linux"
	;;

    *)
	>&2 printf 'Unknown xtensa platform=%s\n' "$platform"
	return 1
	;;
esac

# Pre-zephyr "XTOS" build, testbench,...
case "$platform" in
    mtl|lnl)
	SOF_CC_BASE='clang';;
    *)
	SOF_CC_BASE='xcc';;
esac

# For Zephyr unit tests
case "$platform" in
    imx8*|mtl|lnl)
        ZEPHYR_TOOLCHAIN_VARIANT='xt-clang';;
    *) # The previous, main case/esac already caught invalid input.
       # This won't hurt platforms that don't use Zephyr (it's not even
       # exported).
        ZEPHYR_TOOLCHAIN_VARIANT='xcc';;
esac
