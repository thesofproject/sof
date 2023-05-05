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
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-mtl-nocodec.bin,DEEPBUFFER_FW_DMA_MS=100,\
DEEPBUFFER_D0I3_COMPATIBLE=true"
"cavs-nocodec\;sof-mtl-nocodec-ssp0-ssp2\;PLATFORM=mtl,NUM_DMICS=2,SSP1_ENABLED=false,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-mtl-nocodec.bin,DEEPBUFFER_FW_DMA_MS=100,\
DEEPBUFFER_D0I3_COMPATIBLE=true"

"cavs-nocodec-multicore\;sof-mtl-nocodec-multicore\;PLATFORM=mtl,SSP1_ENABLED=true,SSP0_CORE_ID=0,\
SSP1_CORE_ID=1,SSP2_CORE_ID=2,PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-mtl-nocodec.bin"

# SSP topology for LNL FPGA with lower DMIC IO clock of 19.2MHz
"cavs-nocodec\;sof-lnl-nocodec-fpga\;PLATFORM=lnl,NUM_DMICS=4,PDM1_MIC_A_ENABLE=1,\
PDM1_MIC_B_ENABLE=1,PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-lnl-nocodec-fpga.bin,\
PASSTHROUGH=true,DMIC_IO_CLK=19200000"

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

