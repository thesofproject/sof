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
)
