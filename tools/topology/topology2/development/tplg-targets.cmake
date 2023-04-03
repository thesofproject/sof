# SPDX-License-Identifier: BSD-3-Clause

# Array of "input-file-name;output-file-name;comma separated pre-processor variables"
set(TPLGS
# CAVS SDW topology with passthrough pipelines
"cavs-sdw\;cavs-sdw\;DEEPBUFFER_FW_DMA_MS=100"

# CAVS SDW with SRC gain and mixer support
"cavs-sdw-src-gain-mixin\;cavs-sdw-src-gain-mixin"

# SDW + HDMI topology with passthrough pipelines
"cavs-sdw\;cavs-sdw-hdmi\;DEEPBUFFER_FW_DMA_MS=100"

# CAVS SSP topology for TGL
"cavs-nocodec\;sof-tgl-nocodec\;NUM_DMICS=4,PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-tgl-nocodec.bin,DEEPBUFFER_FW_DMA_MS=100,\
SSP0_MIXER_2LEVEL=1,PLATFORM=tgl"

"cavs-nocodec\;sof-adl-nocodec\;NUM_DMICS=4,PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-adl-nocodec.bin,DEEPBUFFER_FW_DMA_MS=100,\
PLATFORM=adl"

# SDW topology for MTL
"cavs-sdw\;mtl-sdw\;NUM_HDMIS=0"

# SDW + HDMI topology for MTL
"cavs-sdw\;mtl-sdw-hdmi\;PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-mtl-sdw-hdmi.bin"

# SSP topology for MTL
"cavs-nocodec\;sof-mtl-nocodec\;PLATFORM=mtl,NUM_DMICS=4,PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-mtl-nocodec.bin,DEEPBUFFER_FW_DMA_MS=100"
"cavs-nocodec\;sof-mtl-nocodec-ssp0-ssp2\;PLATFORM=mtl,NUM_DMICS=2,SSP1_ENABLED=false,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-mtl-nocodec.bin,DEEPBUFFER_FW_DMA_MS=100"

# CAVS HDA topology with mixer-based efx eq pipelines for HDA and passthrough pipelines for HDMI
"sof-hda-generic\;sof-hda-efx-generic\;HDA_CONFIG=efx,USE_CHAIN_DMA=true,DEEPBUFFER_FW_DMA_MS=100,\
EFX_FIR_PARAMS=passthrough,EFX_IIR_PARAMS=passthrough"
"sof-hda-generic\;sof-hda-efx-generic-2ch\;\
HDA_CONFIG=efx,NUM_DMICS=2,PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-hda-fir-generic-2ch.bin,\
USE_CHAIN_DMA=true,DEEPBUFFER_FW_DMA_MS=100,EFX_FIR_PARAMS=passthrough,EFX_IIR_PARAMS=passthrough"
"sof-hda-generic\;sof-hda-efx-generic-4ch\;\
HDA_CONFIG=efx,NUM_DMICS=4,PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-hda-efx-generic-4ch.bin,USE_CHAIN_DMA=true,\
DEEPBUFFER_FW_DMA_MS=100,EFX_FIR_PARAMS=passthrough,EFX_IIR_PARAMS=passthrough"
# CAVS HDA topology with gain and SRC before mixin for HDA and passthrough pipelines for HDMI
"sof-hda-generic\;sof-hda-src-generic\;HDA_CONFIG=src,USE_CHAIN_DMA=true,DEEPBUFFER_FW_DMA_MS=100"
)

