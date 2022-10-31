# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 Intel Corporation. All rights reserved.

# Sourced script argument is a non-standard bash extension
platform=$1

# Note: This duplicates xtensa-build-zephyr.py

case "$platform" in
    tgl)
	PLATFORM="tgplp"
	XTENSA_CORE="cavs2x_LX6HiFi3_2017_8"
	HOST="xtensa-cnl-elf"
	XTENSA_TOOLS_VERSION="RG-2017.8-linux"
	HAVE_ROM='yes'
	IPC4_CONFIG_OVERLAY="tigerlake_ipc4"
	# default key for TGL
	PLATFORM_PRIVATE_KEY="-D${SIGNING_TOOL}_PRIVATE_KEY=$SOF_TOP/keys/otc_private_key_3k.pem"
	;;
    tgl-h)
	PLATFORM="tgph"
	XTENSA_CORE="cavs2x_LX6HiFi3_2017_8"
	HOST="xtensa-cnl-elf"
	XTENSA_TOOLS_VERSION="RG-2017.8-linux"
	HAVE_ROM='yes'
	# default key for TGL
	PLATFORM_PRIVATE_KEY="-D${SIGNING_TOOL}_PRIVATE_KEY=$SOF_TOP/keys/otc_private_key_3k.pem"
	;;
    imx8)
	PLATFORM="imx8"
	XTENSA_CORE="hifi4_nxp_v3_3_1_2_2017"
	HOST="xtensa-imx-elf"
	XTENSA_TOOLS_VERSION="RG-2017.8-linux"
	;;
    imx8x)
	PLATFORM="imx8x"
	XTENSA_CORE="hifi4_nxp_v3_3_1_2_2017"
	HOST="xtensa-imx-elf"
	XTENSA_TOOLS_VERSION="RG-2017.8-linux"
	;;
    imx8m)
	PLATFORM="imx8m"
	XTENSA_CORE="hifi4_mscale_v0_0_2_2017"
	HOST="xtensa-imx8m-elf"
	XTENSA_TOOLS_VERSION="RG-2017.8-linux"
	;;
    imx8ulp)
	PLATFORM="imx8ulp"
	XTENSA_CORE="hifi4_nxp2_ulp_prod"
	HOST="xtensa-imx8ulp-elf"
	XTENSA_TOOLS_VERSION="RG-2017.8-linux"
	;;
    rn)
	PLATFORM="renoir"
	XTENSA_CORE="ACP_3_1_001_PROD_2019_1"
	HOST="xtensa-rn-elf"
	XTENSA_TOOLS_VERSION="RI-2019.1-linux"
	;;
    rmb)
	PLATFORM="rembrandt"
	ARCH="xtensa"
	XTENSA_CORE="LX7_HiFi5_PROD"
	HOST="xtensa-rmb-elf"
	XTENSA_TOOLS_VERSION="RI-2019.1-linux"
	;;
    mt8186)
	PLATFORM="mt8186"
	XTENSA_CORE="hifi5_7stg_I64D128"
	HOST="xtensa-mt8186-elf"
	XTENSA_TOOLS_VERSION="RI-2020.5-linux"
	;;
    mt8188)
	PLATFORM="mt8188"
	XTENSA_CORE="hifi5_7stg_I64D128"
	HOST="xtensa-mt8188-elf"
	XTENSA_TOOLS_VERSION="RI-2020.5-linux"
	;;
    mt8195)
	PLATFORM="mt8195"
	XTENSA_CORE="hifi4_8195_PROD"
	HOST="xtensa-mt8195-elf"
	XTENSA_TOOLS_VERSION="RI-2019.1-linux"
	;;
esac
