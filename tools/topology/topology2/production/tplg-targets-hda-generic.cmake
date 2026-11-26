# SPDX-License-Identifier: BSD-3-Clause

# Array of "input-file-name;output-file-name;comma separated pre-processor variables"
list(APPEND TPLGS
# HDMI only topology with passthrough pipelines
"sof-hda-generic\;sof-hda-generic-idisp\;"
# HDMI only topology with passthrough pipelines and
# 2 or 4 DMIC, no NHLT blob included in topology
"sof-hda-generic\;sof-hda-generic-idisp-2ch\;NUM_DMICS=2"
"sof-hda-generic\;sof-hda-generic-idisp-4ch\;NUM_DMICS=4"
# HDA topology with mixer-based pipelines for HDA and
# passthrough pipelines for HDMI
"sof-hda-generic\;sof-hda-generic\;HDA_CONFIG=mix,HDA_MIC_ENHANCED_CAPTURE=true,\
EFX_HDA_MIC_TDFB_PARAMS=line2_pass,EFX_HDA_MIC_DRC_PARAMS=passthrough"

"sof-hda-generic\;sof-hda-generic-compr\;HDA_CONFIG=mix,HDA_MIC_ENHANCED_CAPTURE=true,\
EFX_HDA_MIC_TDFB_PARAMS=line2_pass,EFX_HDA_MIC_DRC_PARAMS=passthrough,COMPRESSED=true"

"sof-hda-generic\;sof-hda-generic-dax\;HDA_CONFIG=dax,HDA_MIC_ENHANCED_CAPTURE=true,\
EFX_HDA_MIC_TDFB_PARAMS=line2_pass,EFX_HDA_MIC_DRC_PARAMS=passthrough,DOLBY_DAX_CORE_ID=1"

# HDA topology with mixer-based pipelines for HDA and
# passthrough pipelines for HDMI and
# 2 or 4 DMIC, no NHLT blob included in topology
"sof-hda-generic\;sof-hda-generic-1ch\;HDA_CONFIG=mix,NUM_DMICS=1,PDM0_MIC_B_ENABLE=0"

"sof-hda-generic\;sof-hda-generic-2ch\;HDA_CONFIG=mix,NUM_DMICS=2,\
DMIC0_ENHANCED_CAPTURE=true,EFX_DMIC0_TDFB_PARAMS=line2_generic_pm10deg,\
EFX_DMIC0_DRC_PARAMS=dmic_default"

"sof-hda-generic\;sof-hda-generic-4ch\;HDA_CONFIG=mix,NUM_DMICS=4,\
PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,DMIC0_ENHANCED_CAPTURE=true,\
EFX_DMIC0_TDFB_PARAMS=line4_pass,EFX_DMIC0_DRC_PARAMS=dmic_default"

"sof-hda-generic\;sof-hda-generic-2ch-dax\;HDA_CONFIG=dax,NUM_DMICS=2,\
DMIC0_ENHANCED_CAPTURE=true,EFX_DMIC0_TDFB_PARAMS=line2_generic_pm10deg,\
EFX_DMIC0_DRC_PARAMS=dmic_default,DOLBY_DAX_CORE_ID=1"

# HDA topology with mixer-based pipelines for HDA and
# passthrough pipelines for HDMI and
# 2 or 4 DMIC, no NHLT blob included in topology

# Note: If the alsatplg plugins for NHLT are not available, the NHLT blobs will
# not be added to the topologies below.

# Topologies for CAVS2.5 architecture
# HDMI + DMICs
"sof-hda-generic\;sof-hda-generic-cavs25-idisp-2ch\;NUM_DMICS=2,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-hda-generic-cavs25-idisp-2ch.bin"
"sof-hda-generic\;sof-hda-generic-cavs25-idisp-4ch\;NUM_DMICS=4,\
PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-hda-generic-cavs25-idisp-4ch.bin"
# HDA + HDMI + DMICs
"sof-hda-generic\;sof-hda-generic-cavs25-1ch\;HDA_CONFIG=mix,NUM_DMICS=1,PDM0_MIC_B_ENABLE=0,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-hda-generic-cavs25-1ch.bin"

"sof-hda-generic\;sof-hda-generic-cavs25-2ch\;HDA_CONFIG=mix,NUM_DMICS=2,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-hda-generic-cavs25-2ch.bin,\
DMIC0_ENHANCED_CAPTURE=true,EFX_DMIC0_TDFB_PARAMS=line2_generic_pm10deg,\
EFX_DMIC0_DRC_PARAMS=dmic_default"

"sof-hda-generic\;sof-hda-generic-cavs25-4ch\;HDA_CONFIG=mix,NUM_DMICS=4,\
PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-hda-generic-cavs25-4ch.bin,\
DMIC0_ENHANCED_CAPTURE=true,\
EFX_DMIC0_TDFB_PARAMS=line4_pass,EFX_DMIC0_DRC_PARAMS=dmic_default"

"sof-hda-generic\;sof-hda-generic-cavs25-2ch-dax\;HDA_CONFIG=dax,NUM_DMICS=2,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-hda-generic-cavs25-2ch-dax.bin,\
DMIC0_ENHANCED_CAPTURE=true,EFX_DMIC0_TDFB_PARAMS=line2_generic_pm10deg,\
EFX_DMIC0_DRC_PARAMS=dmic_default,DOLBY_DAX_CORE_ID=1"

# Topologies for ACE1 and ACE2 architectures
# HDMI + DMICs
"sof-hda-generic\;sof-hda-generic-ace1-idisp-2ch\;PLATFORM=mtl,NUM_DMICS=2,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-hda-generic-ace1-idisp-2ch.bin"
"sof-hda-generic\;sof-hda-generic-ace1-idisp-4ch\;PLATFORM=mtl,NUM_DMICS=4,\
PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-hda-generic-ace1-idisp-4ch.bin"
# HDA + HDMI + DMICs
"sof-hda-generic\;sof-hda-generic-ace1-1ch\;PLATFORM=mtl,HDA_CONFIG=mix,NUM_DMICS=1,PDM0_MIC_B_ENABLE=0,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-hda-generic-ace1-1ch.bin"

"sof-hda-generic\;sof-hda-generic-ace1-2ch\;PLATFORM=mtl,HDA_CONFIG=mix,NUM_DMICS=2,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-hda-generic-ace1-2ch.bin,\
DMIC0_ENHANCED_CAPTURE=true,EFX_DMIC0_TDFB_PARAMS=line2_generic_pm10deg,\
EFX_DMIC0_DRC_PARAMS=dmic_default"

"sof-hda-generic\;sof-hda-generic-ace1-4ch\;PLATFORM=mtl,HDA_CONFIG=mix,NUM_DMICS=4,\
PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-hda-generic-ace1-4ch.bin,DMIC0_ENHANCED_CAPTURE=true,\
EFX_DMIC0_TDFB_PARAMS=line4_pass,EFX_DMIC0_DRC_PARAMS=dmic_default"

"sof-hda-generic\;sof-hda-generic-ace1-2ch-dax\;PLATFORM=mtl,HDA_CONFIG=dax,NUM_DMICS=2,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-hda-generic-ace1-2ch-dax.bin,\
DMIC0_ENHANCED_CAPTURE=true,EFX_DMIC0_TDFB_PARAMS=line2_generic_pm10deg,\
EFX_DMIC0_DRC_PARAMS=dmic_default,DOLBY_DAX_CORE_ID=1"

# Topologies for ACE3 architecture
# HDMI + DMICs
"sof-hda-generic\;sof-hda-generic-ace3-idisp-2ch\;PLATFORM=ptl,NUM_DMICS=2,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-hda-generic-ace3-idisp-2ch.bin"

"sof-hda-generic\;sof-hda-generic-ace3-idisp-4ch\;PLATFORM=ptl,NUM_DMICS=4,\
PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-hda-generic-ace3-idisp-4ch.bin"

# HDA + HDMI + DMICs
"sof-hda-generic\;sof-hda-generic-ace3-1ch\;PLATFORM=ptl,HDA_CONFIG=mix,NUM_DMICS=1,PDM0_MIC_B_ENABLE=0,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-hda-generic-ace3-1ch.bin"

"sof-hda-generic\;sof-hda-generic-ace3-2ch\;PLATFORM=ptl,HDA_CONFIG=mix,NUM_DMICS=2,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-hda-generic-ace3-2ch.bin,\
DMIC0_ENHANCED_CAPTURE=true,EFX_DMIC0_TDFB_PARAMS=line2_generic_pm10deg,\
EFX_DMIC0_DRC_PARAMS=dmic_default"

"sof-hda-generic\;sof-hda-generic-ace3-4ch\;PLATFORM=ptl,HDA_CONFIG=mix,NUM_DMICS=4,\
PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-hda-generic-ace3-4ch.bin,DMIC0_ENHANCED_CAPTURE=true,\
EFX_DMIC0_TDFB_PARAMS=line4_pass,EFX_DMIC0_DRC_PARAMS=dmic_default"

"sof-hda-generic\;sof-hda-generic-ace3-2ch-dax\;PLATFORM=ptl,HDA_CONFIG=dax,NUM_DMICS=2,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-hda-generic-ace3-2ch-dax.bin,\
DMIC0_ENHANCED_CAPTURE=true,EFX_DMIC0_TDFB_PARAMS=line2_generic_pm10deg,\
EFX_DMIC0_DRC_PARAMS=dmic_default,DOLBY_DAX_CORE_ID=1"

# Platform independent function topologies, shared by all platforms. The HDA
# codec and HDMI topologies have no NHLT blob, so sof_hda_get_tplg_files() in
# the Linux kernel requests them without a platform in the name.
"sof-hda-generic\;sof-hda-id4\;HDA_CONFIG=mix,NUM_HDMIS=0,\
HDA_MIC_ENHANCED_CAPTURE=true,EFX_HDA_MIC_TDFB_PARAMS=line2_pass,\
EFX_HDA_MIC_DRC_PARAMS=passthrough"

"sof-hda-generic\;sof-hdmi-pcm3-id1\;PLATFORM=ptl"

# Function topologies, ACE1, named the way sof_hda_get_tplg_files() in the Linux
# kernel requests them. Each one enables a single function of
# sof-hda-generic.conf and disables the others (HDA_CONFIG=none, NUM_DMICS=0,
# NUM_HDMIS=0, HDMI_IN_CAPTURE=false, ADD_BT=false), the way the sof-sdca-*
# topologies do with cavs-sdw.conf. They are loaded next to each other, so
# their PCM and pipeline IDs must not collide.
"sof-hda-generic\;sof-mtl-dmic-2ch-id6\;PLATFORM=mtl,NUM_HDMIS=0,NUM_DMICS=2,\
DMIC0_HOST_PIPELINE_ID=100,DMIC0_DAI_PIPELINE_ID=101,\
DMIC0_DAI_PIPELINE_SRC=module-copier.101.2,\
DMIC0_ENHANCED_CAPTURE=true,EFX_DMIC0_TDFB_PARAMS=line2_generic_pm10deg,\
EFX_DMIC0_DRC_PARAMS=dmic_default"

"sof-hda-generic\;sof-mtl-dmic-4ch-id6\;PLATFORM=mtl,NUM_HDMIS=0,NUM_DMICS=4,\
DMIC0_HOST_PIPELINE_ID=100,DMIC0_DAI_PIPELINE_ID=101,\
DMIC0_DAI_PIPELINE_SRC=module-copier.101.2,\
PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,\
DMIC0_ENHANCED_CAPTURE=true,EFX_DMIC0_TDFB_PARAMS=line4_pass,\
EFX_DMIC0_DRC_PARAMS=dmic_default"

"sof-hda-generic\;sof-mtl-ssp6-hdmiin-id14\;PLATFORM=mtl,NUM_HDMIS=0,\
HDMI_IN_CAPTURE=true,HDMI_IN_1_INDEX=1,HDMI_IN_2_INDEX=2,HDMI_IN_1_NAME=SSP1-HDMI,\
HDMI_IN_1_ID=13,HDMI_IN_2_ID=14,\
HDMI_IN_1_CP_HOST_PIPELINE_ID=210,HDMI_IN_1_CP_DAI_PIPELINE_ID=211,\
HDMI_IN_2_CP_HOST_PIPELINE_ID=212,HDMI_IN_2_CP_DAI_PIPELINE_ID=213,\
HDMI_IN_1_PCM_ID=7,HDMI_IN_2_PCM_ID=8"

"sof-hda-generic\;sof-mtl-ssp2-bt-id8\;PLATFORM=mtl,NUM_HDMIS=0,ADD_BT=true,BT_DEEP_BUFFER=true,\
BT_PCM_ID=20,BT_ID=8,BT_PCM_NAME=Bluetooth,\
BT_PB_HOST_PIPELINE_ID=200,BT_PB_DAI_PIPELINE_ID=201,BT_CP_HOST_PIPELINE_ID=202,BT_CP_DAI_PIPELINE_ID=203"

# Function topologies, ACE3, named the way sof_hda_get_tplg_files() in the Linux
# kernel requests them. Each one enables a single function of
# sof-hda-generic.conf and disables the others (HDA_CONFIG=none, NUM_DMICS=0,
# NUM_HDMIS=0, HDMI_IN_CAPTURE=false, ADD_BT=false), the way the sof-sdca-*
# topologies do with cavs-sdw.conf. They are loaded next to each other, so
# their PCM and pipeline IDs must not collide.
"sof-hda-generic\;sof-ptl-dmic-2ch-id6\;PLATFORM=ptl,NUM_HDMIS=0,NUM_DMICS=2,\
DMIC0_HOST_PIPELINE_ID=100,DMIC0_DAI_PIPELINE_ID=101,\
DMIC0_DAI_PIPELINE_SRC=module-copier.101.2,\
DMIC0_ENHANCED_CAPTURE=true,EFX_DMIC0_TDFB_PARAMS=line2_generic_pm10deg,\
EFX_DMIC0_DRC_PARAMS=dmic_default"

"sof-hda-generic\;sof-ptl-dmic-4ch-id6\;PLATFORM=ptl,NUM_HDMIS=0,NUM_DMICS=4,\
DMIC0_HOST_PIPELINE_ID=100,DMIC0_DAI_PIPELINE_ID=101,\
DMIC0_DAI_PIPELINE_SRC=module-copier.101.2,\
PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,\
DMIC0_ENHANCED_CAPTURE=true,EFX_DMIC0_TDFB_PARAMS=line4_pass,\
EFX_DMIC0_DRC_PARAMS=dmic_default"

"sof-hda-generic\;sof-ptl-ssp6-hdmiin-id14\;PLATFORM=ptl,NUM_HDMIS=0,\
HDMI_IN_CAPTURE=true,HDMI_IN_1_INDEX=1,HDMI_IN_2_INDEX=2,HDMI_IN_1_NAME=SSP1-HDMI,\
HDMI_IN_1_ID=13,HDMI_IN_2_ID=14,\
HDMI_IN_1_CP_HOST_PIPELINE_ID=210,HDMI_IN_1_CP_DAI_PIPELINE_ID=211,\
HDMI_IN_2_CP_HOST_PIPELINE_ID=212,HDMI_IN_2_CP_DAI_PIPELINE_ID=213,\
HDMI_IN_1_PCM_ID=7,HDMI_IN_2_PCM_ID=8"

# Function topologies, ACE2 (LNL), named the way sof_hda_get_tplg_files() in the Linux
# kernel requests them. See the ACE1 ones above.
"sof-hda-generic\;sof-lnl-dmic-2ch-id6\;PLATFORM=lnl,NUM_HDMIS=0,NUM_DMICS=2,\
DMIC0_HOST_PIPELINE_ID=100,DMIC0_DAI_PIPELINE_ID=101,\
DMIC0_DAI_PIPELINE_SRC=module-copier.101.2,\
DMIC0_ENHANCED_CAPTURE=true,EFX_DMIC0_TDFB_PARAMS=line2_generic_pm10deg,\
EFX_DMIC0_DRC_PARAMS=dmic_default"

"sof-hda-generic\;sof-lnl-dmic-4ch-id6\;PLATFORM=lnl,NUM_HDMIS=0,NUM_DMICS=4,\
DMIC0_HOST_PIPELINE_ID=100,DMIC0_DAI_PIPELINE_ID=101,\
DMIC0_DAI_PIPELINE_SRC=module-copier.101.2,\
PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,\
DMIC0_ENHANCED_CAPTURE=true,EFX_DMIC0_TDFB_PARAMS=line4_pass,\
EFX_DMIC0_DRC_PARAMS=dmic_default"

"sof-hda-generic\;sof-lnl-ssp2-bt-id8\;PLATFORM=lnl,NUM_HDMIS=0,ADD_BT=true,BT_DEEP_BUFFER=true,\
BT_PCM_ID=20,BT_ID=8,BT_PCM_NAME=Bluetooth,\
BT_PB_HOST_PIPELINE_ID=200,BT_PB_DAI_PIPELINE_ID=201,BT_CP_HOST_PIPELINE_ID=202,BT_CP_DAI_PIPELINE_ID=203"

"sof-hda-generic\;sof-lnl-ssp6-hdmiin-id14\;PLATFORM=lnl,NUM_HDMIS=0,\
HDMI_IN_CAPTURE=true,HDMI_IN_1_INDEX=1,HDMI_IN_2_INDEX=2,HDMI_IN_1_NAME=SSP1-HDMI,\
HDMI_IN_1_ID=13,HDMI_IN_2_ID=14,\
HDMI_IN_1_CP_HOST_PIPELINE_ID=210,HDMI_IN_1_CP_DAI_PIPELINE_ID=211,\
HDMI_IN_2_CP_HOST_PIPELINE_ID=212,HDMI_IN_2_CP_DAI_PIPELINE_ID=213,\
HDMI_IN_1_PCM_ID=7,HDMI_IN_2_PCM_ID=8"

# Function topologies, ACE4 (NVL), named the way sof_hda_get_tplg_files() in the Linux
# kernel requests them. See the ACE1 ones above.
"sof-hda-generic\;sof-nvl-dmic-2ch-id6\;PLATFORM=nvl,NUM_HDMIS=0,NUM_DMICS=2,\
DMIC0_HOST_PIPELINE_ID=100,DMIC0_DAI_PIPELINE_ID=101,\
DMIC0_DAI_PIPELINE_SRC=module-copier.101.2,\
DMIC0_ENHANCED_CAPTURE=true,EFX_DMIC0_TDFB_PARAMS=line2_generic_pm10deg,\
EFX_DMIC0_DRC_PARAMS=dmic_default"

"sof-hda-generic\;sof-nvl-dmic-4ch-id6\;PLATFORM=nvl,NUM_HDMIS=0,NUM_DMICS=4,\
DMIC0_HOST_PIPELINE_ID=100,DMIC0_DAI_PIPELINE_ID=101,\
DMIC0_DAI_PIPELINE_SRC=module-copier.101.2,\
PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,\
DMIC0_ENHANCED_CAPTURE=true,EFX_DMIC0_TDFB_PARAMS=line4_pass,\
EFX_DMIC0_DRC_PARAMS=dmic_default"

"sof-hda-generic\;sof-nvl-ssp2-bt-id8\;PLATFORM=nvl,NUM_HDMIS=0,ADD_BT=true,BT_DEEP_BUFFER=true,\
BT_PCM_ID=20,BT_ID=8,BT_PCM_NAME=Bluetooth,\
BT_PB_HOST_PIPELINE_ID=200,BT_PB_DAI_PIPELINE_ID=201,BT_CP_HOST_PIPELINE_ID=202,BT_CP_DAI_PIPELINE_ID=203"

"sof-hda-generic\;sof-nvl-ssp6-hdmiin-id14\;PLATFORM=nvl,NUM_HDMIS=0,\
HDMI_IN_CAPTURE=true,HDMI_IN_1_INDEX=1,HDMI_IN_2_INDEX=2,HDMI_IN_1_NAME=SSP1-HDMI,\
HDMI_IN_1_ID=13,HDMI_IN_2_ID=14,\
HDMI_IN_1_CP_HOST_PIPELINE_ID=210,HDMI_IN_1_CP_DAI_PIPELINE_ID=211,\
HDMI_IN_2_CP_HOST_PIPELINE_ID=212,HDMI_IN_2_CP_DAI_PIPELINE_ID=213,\
HDMI_IN_1_PCM_ID=7,HDMI_IN_2_PCM_ID=8"
)

list(APPEND SYMLINKS
# ARL and WCL use the MTL and PTL HDA function topologies
"sof-arl-dmic-2ch-id6\;sof-mtl-dmic-2ch-id6"
"sof-arl-dmic-4ch-id6\;sof-mtl-dmic-4ch-id6"
"sof-arl-ssp2-bt-id8\;sof-mtl-ssp2-bt-id8"
"sof-arl-ssp6-hdmiin-id14\;sof-mtl-ssp6-hdmiin-id14"
"sof-wcl-dmic-2ch-id6\;sof-ptl-dmic-2ch-id6"
"sof-wcl-dmic-4ch-id6\;sof-ptl-dmic-4ch-id6"
"sof-wcl-ssp2-bt-id8\;sof-ptl-ssp2-bt-id8"
"sof-wcl-ssp6-hdmiin-id14\;sof-ptl-ssp6-hdmiin-id14"
)
