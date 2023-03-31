# SPDX-License-Identifier: BSD-3-Clause

# Array of "input-file-name;output-file-name;comma separated pre-processor variables"
set(TPLGS
# HDMI only topology with passthrough pipelines
"sof-hda-generic\;sof-hda-generic-idisp\;USE_CHAIN_DMA=true,DEEPBUFFER_FW_DMA_MS=100"
# HDA topology with mixer-based pipelines for HDA and passthrough pipelines for HDMI
"sof-hda-generic\;sof-hda-generic\;HDA_CONFIG=mix,USE_CHAIN_DMA=true,DEEPBUFFER_FW_DMA_MS=100"
# If the alsatplg plugins for NHLT are not available, the NHLT blobs will not be added to the
# topologies below.
"sof-hda-generic\;sof-hda-generic-4ch\;PLATFORM=mtl,\
HDA_CONFIG=mix,NUM_DMICS=4,PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,PREPROCESS_PLUGINS=nhlt,\
NHLT_BIN=nhlt-sof-hda-generic-4ch.bin,USE_CHAIN_DMA=true,DEEPBUFFER_FW_DMA_MS=100"
"sof-hda-generic\;sof-hda-generic-2ch\;PLATFORM=mtl,\
HDA_CONFIG=mix,NUM_DMICS=2,PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-hda-generic-2ch.bin,\
USE_CHAIN_DMA=true,DEEPBUFFER_FW_DMA_MS=100"

# SDW + DMIC topology with passthrough pipelines
# We will change NUM_HDMIS to 3 once HDMI is enabled on MTL RVP
"cavs-sdw\;sof-mtl-rt711-4ch\;PLATFORM=mtl,NUM_DMICS=4,DMIC0_ID=2,DMIC1_ID=3,NUM_HDMIS=0,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-mtl-rt711-4ch.bin,DEEPBUFFER_FW_DMA_MS=100"

"cavs-rt5682\;sof-mtl-max98357a-rt5682\;PLATFORM=mtl,NUM_DMICS=4,PDM1_MIC_A_ENABLE=1,\
PDM1_MIC_B_ENABLE=1,DMIC0_PCM_ID=99,PREPROCESS_PLUGINS=nhlt,\
NHLT_BIN=nhlt-sof-mtl-max98357a-rt5682.bin,DEEPBUFFER_FW_DMA_MS=10,INCLUDE_ECHO_REF=true,\
BT_NAME=SSP2-BT,BT_ID=8,BT_PCM_NAME=Bluetooth"

"cavs-rt5682\;sof-mtl-max98357a-rt5682-ssp2-ssp0\;PLATFORM=mtl,NUM_DMICS=4,PDM1_MIC_A_ENABLE=1,\
PDM1_MIC_B_ENABLE=1,DMIC0_PCM_ID=99,PREPROCESS_PLUGINS=nhlt,\
NHLT_BIN=nhlt-sof-mtl-max98357a-rt5682.bin,DEEPBUFFER_FW_DMA_MS=10,HEADSET_SSP_DAI_INDEX=2,\
SPEAKER_SSP_DAI_INDEX=0,HEADSET_CODEC_NAME=SSP2-Codec,SPEAKER_CODEC_NAME=SSP0-Codec,\
BT_NAME=SSP1-BT,BT_INDEX=1,BT_ID=8,BT_PCM_NAME=Bluetooth,INCLUDE_ECHO_REF=true"
)
