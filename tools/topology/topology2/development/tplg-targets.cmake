# SPDX-License-Identifier: BSD-3-Clause

# Array of "input-file-name;output-file-name;comma separated pre-processor variables"
set(TPLGS
# CAVS SDW topology with passthrough pipelines
"cavs-sdw\;cavs-sdw\;"

# CAVS SDW with SRC gain and mixer support
"cavs-sdw-src-gain-mixin\;cavs-sdw-src-gain-mixin"

# SDW + HDMI topology with passthrough pipelines
"cavs-sdw\;cavs-sdw-hdmi\;"

# CAVS SSP topology for TGL
# multicore disabled due to SOF issue #8942
"cavs-nocodec\;sof-tgl-nocodec\;NUM_DMICS=4,PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-tgl-nocodec.bin,\
SSP1_PCM_CORE_ID=0,SSP2_PCM_CORE_ID=0,\
SSP0_MIXER_2LEVEL=1,PLATFORM=tgl"

# multicore disabled due to SOF issue #8942
"cavs-nocodec\;sof-adl-nocodec\;NUM_DMICS=4,PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,\
SSP1_PCM_CORE_ID=0,SSP2_PCM_CORE_ID=0,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-adl-nocodec.bin,PLATFORM=adl"

# SDW topology for MTL
"cavs-sdw\;mtl-sdw\;NUM_HDMIS=0"

# SDW + HDMI topology for MTL
"cavs-sdw\;mtl-sdw-hdmi\;PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-mtl-sdw-hdmi.bin"

# SSP topology for MTL
"cavs-nocodec\;sof-mtl-nocodec\;PLATFORM=mtl,NUM_DMICS=4,PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-mtl-nocodec.bin"
"cavs-nocodec\;sof-mtl-nocodec-ssp0-ssp2\;PLATFORM=mtl,NUM_DMICS=2,SSP1_ENABLED=false,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-mtl-nocodec-ssp0-ssp2.bin"

# SSP topology for LNL
"cavs-nocodec\;sof-lnl-nocodec\;PLATFORM=lnl,NUM_DMICS=4,PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-lnl-nocodec.bin"

# SSP topology for LNL FPGA with lower DMIC IO clock of 19.2MHz, 2ch PDM1 enabled
"cavs-nocodec\;sof-lnl-nocodec-fpga-2ch-pdm1\;PLATFORM=lnl,NUM_DMICS=2,PDM1_MIC_A_ENABLE=1,\
PDM1_MIC_B_ENABLE=1,PDM0_MIC_A_ENABLE=0,PDM0_MIC_B_ENABLE=0,PREPROCESS_PLUGINS=nhlt,\
NHLT_BIN=nhlt-sof-lnl-nocodec-fpga-2ch-pdm1.bin,PASSTHROUGH=true,DMIC_IO_CLK=19200000"

# SSP topology for LNL FPGA with lower DMIC IO clock of 19.2MHz, 2ch PDM0 enabled
"cavs-nocodec\;sof-lnl-nocodec-fpga-2ch-pdm0\;PLATFORM=lnl,NUM_DMICS=2,PREPROCESS_PLUGINS=nhlt,\
NHLT_BIN=nhlt-sof-lnl-nocodec-fpga-2ch-pdm0.bin,PASSTHROUGH=true,DMIC_IO_CLK=19200000"

# SSP topology for LNL FPGA with lower DMIC IO clock of 19.2MHz, 4ch both PDM0 and PDM1 enabled
"cavs-nocodec\;sof-lnl-nocodec-fpga-4ch\;PLATFORM=lnl,NUM_DMICS=4,PDM1_MIC_A_ENABLE=1,\
PDM1_MIC_B_ENABLE=1,PREPROCESS_PLUGINS=nhlt,\
NHLT_BIN=nhlt-sof-lnl-nocodec-fpga-4ch.bin,PASSTHROUGH=true,DMIC_IO_CLK=19200000"

"cavs-sdw\;sof-lnl-fpga-rt711-l0\;PLATFORM=lnl,NUM_HDMIS=0,PASSTHROUGH=true"

# SSP topology for PTL
"cavs-nocodec\;sof-ptl-nocodec\;PLATFORM=ptl,NUM_DMICS=4,PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-ptl-nocodec.bin"

# SSP topology for PTL with 96 kHz DMIC
"cavs-nocodec\;sof-ptl-nocodec-dmic-4ch-96k\;PLATFORM=ptl,NUM_DMICS=4,PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,\
DMIC0_RATE=96000,PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-ptl-nocodec-dmic-4ch-96k.bin"

# SSP topology for PTL with 96 kHz DMIC and 96 kHz SSP0
"cavs-nocodec\;sof-ptl-nocodec-dmic-4ch-96k-ssp0-96k\;PLATFORM=ptl,NUM_DMICS=4,\
PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,DMIC0_RATE=96000,SSP0_RATE=96000,PASSTHROUGH=true,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-ptl-nocodec-dmic-4ch-96k-ssp0-96k.bin"

# SSP topology for PTL FPGA with lower DMIC IO clock of 19.2MHz, 2ch PDM0 enabled
"cavs-nocodec\;sof-ptl-nocodec-fpga-2ch-pdm0\;PLATFORM=ptl,NUM_DMICS=2,PREPROCESS_PLUGINS=nhlt,\
NHLT_BIN=nhlt-sof-ptl-nocodec-fpga-2ch-pdm0.bin,PASSTHROUGH=true,DMIC_IO_CLK=19200000"

# SSP topology for PTL FPGA with lower DMIC IO clock of 19.2MHz, 4ch both PDM0 and PDM1 enabled
"cavs-nocodec\;sof-ptl-nocodec-fpga-4ch\;PLATFORM=ptl,NUM_DMICS=4,PDM1_MIC_A_ENABLE=1,\
PDM1_MIC_B_ENABLE=1,PREPROCESS_PLUGINS=nhlt,\
NHLT_BIN=nhlt-sof-ptl-nocodec-fpga-4ch.bin,PASSTHROUGH=true,DMIC_IO_CLK=19200000"

# SSP topology for PTL FPGA with lower DMIC IO clock of 19.2MHz, 4ch both PDM0 and PDM1 enabled, with 96 kHz DMIC
"cavs-nocodec\;sof-ptl-nocodec-fpga-dmic-4ch-96k\;PLATFORM=ptl,NUM_DMICS=4,PDM1_MIC_A_ENABLE=1,\
PDM1_MIC_B_ENABLE=1,DMIC0_RATE=96000,PREPROCESS_PLUGINS=nhlt,\
NHLT_BIN=nhlt-sof-ptl-nocodec-fpga-dmic-4ch-96k.bin,PASSTHROUGH=true,DMIC_IO_CLK=19200000"

# SSP topology for PTL FPGA, same as previous but with 96 kHz SSP0
"cavs-nocodec\;sof-ptl-nocodec-fpga-dmic-4ch-96k-ssp0-96k\;PLATFORM=ptl,NUM_DMICS=4,PDM1_MIC_A_ENABLE=1,\
PDM1_MIC_B_ENABLE=1,DMIC0_RATE=96000,SSP0_RATE=96000,PREPROCESS_PLUGINS=nhlt,\
NHLT_BIN=nhlt-sof-ptl-nocodec-fpga-dmic-4ch-96k-ssp0-96k.bin,PASSTHROUGH=true,DMIC_IO_CLK=19200000"

# Topology for PTL with max98357a and rt5682
"cavs-rt5682\;sof-ptl-max98357a-rt5682-ssp2-ssp0\;PLATFORM=ptl,NUM_DMICS=4,PDM1_MIC_A_ENABLE=1,\
PDM1_MIC_B_ENABLE=1,DMIC0_PCM_ID=99,PREPROCESS_PLUGINS=nhlt,\
NHLT_BIN=nhlt-sof-ptl-max98357a-rt5682-ssp2-ssp0.bin,DEEPBUFFER_FW_DMA_MS=10,HEADSET_SSP_DAI_INDEX=2,\
SPK_ID=6,SPEAKER_SSP_DAI_INDEX=0,HEADSET_CODEC_NAME=SSP2-Codec,SPEAKER_CODEC_NAME=SSP0-Codec,\
BT_NAME=SSP1-BT,BT_INDEX=1,BT_ID=7,BT_PCM_NAME=Bluetooth,INCLUDE_ECHO_REF=true,DEEP_BUF_SPK=true"

# CAVS HDA topology with mixer-based efx eq pipelines for HDA and passthrough pipelines for HDMI
"sof-hda-generic\;sof-hda-efx-generic\;HDA_CONFIG=efx,HDA_MIC_ENHANCED_CAPTURE=true,\
EFX_FIR_PARAMS=passthrough,EFX_IIR_PARAMS=passthrough,EFX_DRC_PARAMS=passthrough,\
EFX_HDA_MIC_TDFB_PARAMS=line2_pass,EFX_HDA_MIC_DRC_PARAMS=passthrough"

"sof-hda-generic\;sof-hda-efx-generic-2ch\;\
HDA_CONFIG=efx,NUM_DMICS=2,\
EFX_FIR_PARAMS=passthrough,EFX_IIR_PARAMS=passthrough,EFX_DRC_PARAMS=passthrough,\
DMIC0_ENHANCED_CAPTURE=true,EFX_DMIC0_TDFB_PARAMS=line2_generic_pm10deg,EFX_DMIC0_DRC_PARAMS=dmic_default"

"sof-hda-generic\;sof-hda-efx-generic-4ch\;\
HDA_CONFIG=efx,NUM_DMICS=4,PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,\
EFX_FIR_PARAMS=passthrough,EFX_IIR_PARAMS=passthrough,EFX_DRC_PARAMS=passthrough,\
DMIC0_ENHANCED_CAPTURE=true,EFX_DMIC0_TDFB_PARAMS=line4_pass,EFX_DMIC0_DRC_PARAMS=dmic_default,\
DMIC0_PCM_CHANNELS=4"

"sof-hda-generic\;sof-hda-efx-generic-4to2ch\;\
HDA_CONFIG=efx,NUM_DMICS=4,PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,\
EFX_FIR_PARAMS=passthrough,EFX_IIR_PARAMS=passthrough,EFX_DRC_PARAMS=passthrough,\
DMIC0_ENHANCED_CAPTURE=true,EFX_DMIC0_TDFB_PARAMS=line4_generic_pm10deg,\
EFX_DMIC0_DRC_PARAMS=dmic_default,DMIC0_PCM_CHANNELS=2"

"sof-hda-generic\;sof-hda-efx-mbdrc-generic\;HDA_CONFIG=efx,HDA_MIC_ENHANCED_CAPTURE=true,\
EFX_FIR_PARAMS=passthrough,EFX_IIR_PARAMS=passthrough,EFX_DRC_COMPONENT=multiband,\
EFX_MBDRC_PARAMS=passthrough,EFX_HDA_MIC_TDFB_PARAMS=line2_pass,EFX_HDA_MIC_DRC_PARAMS=passthrough"

"sof-hda-generic\;sof-hda-efx-mbdrc-generic-2ch\;\
HDA_CONFIG=efx,NUM_DMICS=2,EFX_FIR_PARAMS=passthrough,EFX_IIR_PARAMS=passthrough,\
EFX_DRC_COMPONENT=multiband,EFX_MBDRC_PARAMS=passthrough,DMIC0_ENHANCED_CAPTURE=true,\
EFX_DMIC0_TDFB_PARAMS=line2_generic_pm10deg,EFX_DMIC0_DRC_PARAMS=dmic_default"

"sof-hda-generic\;sof-hda-efx-mbdrc-generic-4ch\;\
HDA_CONFIG=efx,NUM_DMICS=4,PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,EFX_FIR_PARAMS=passthrough,\
EFX_IIR_PARAMS=passthrough,EFX_DRC_COMPONENT=multiband,EFX_MBDRC_PARAMS=passthrough,\
DMIC0_ENHANCED_CAPTURE=true,EFX_DMIC0_TDFB_PARAMS=line4_pass,EFX_DMIC0_DRC_PARAMS=dmic_default,\
DMIC0_PCM_CHANNELS=4"

"sof-hda-generic\;sof-hda-efx-mbdrc-generic-4to2ch\;\
HDA_CONFIG=efx,NUM_DMICS=4,PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,EFX_FIR_PARAMS=passthrough,\
EFX_IIR_PARAMS=passthrough,EFX_DRC_COMPONENT=multiband,EFX_MBDRC_PARAMS=passthrough,\
DMIC0_ENHANCED_CAPTURE=true,EFX_DMIC0_TDFB_PARAMS=line4_generic_pm10deg,\
EFX_DMIC0_DRC_PARAMS=dmic_default,DMIC0_PCM_CHANNELS=2"

# With 16 kHz DMIC1
"sof-hda-generic\;sof-hda-generic-cavs25-4ch-48k-16k\;HDA_CONFIG=mix,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-hda-generic-cavs25-4ch-48k-16k.bin,\
NUM_DMICS=4,PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,DMIC1_ENABLE=passthrough,DMIC1_RATE=16000"

"sof-hda-generic\;sof-hda-generic-ace1-4ch-48k-16k\;HDA_CONFIG=mix,PLATFORM=mtl,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-hda-generic-ace1-4ch-48k-16k.bin,\
NUM_DMICS=4,PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,DMIC1_ENABLE=passthrough,DMIC1_RATE=16000"

# With 4ch 96 kHz DMIC1 and HDA
"sof-hda-generic\;sof-hda-generic-cavs25-4ch-48k-96k\;HDA_CONFIG=mix,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-hda-generic-cavs25-4ch-48k-96k.bin,\
DEEPBUFFER_FW_DMA_MS=false,HDA_ANALOG_CAPTURE_RATE=96000,HDA_ANALOG_PLAYBACK_RATE=96000,\
NUM_DMICS=4,PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,DMIC1_ENABLE=passthrough,\
DMIC1_RATE=96000,DMIC1_PCM_ID=22"

"sof-hda-generic\;sof-hda-generic-ace1-4ch-48k-96k\;HDA_CONFIG=mix,PLATFORM=mtl,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-hda-generic-ace1-4ch-48k-96k.bin,\
DEEPBUFFER_FW_DMA_MS=false,HDA_ANALOG_CAPTURE_RATE=96000,HDA_ANALOG_PLAYBACK_RATE=96000,\
NUM_DMICS=4,PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,DMIC1_ENABLE=passthrough,\
DMIC1_RATE=96000,DMIC1_PCM_ID=22"

# With 2ch 96 kHz DMIC1 and HDA
"sof-hda-generic\;sof-hda-generic-cavs25-2ch-48k-96k\;HDA_CONFIG=mix,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-hda-generic-cavs25-2ch-48k-96k.bin,\
DEEPBUFFER_FW_DMA_MS=false,HDA_ANALOG_CAPTURE_RATE=96000,HDA_ANALOG_PLAYBACK_RATE=96000,\
NUM_DMICS=2,PDM1_MIC_A_ENABLE=0,PDM1_MIC_B_ENABLE=0,DMIC1_ENABLE=passthrough,\
DMIC1_RATE=96000,DMIC1_PCM_ID=22"

"sof-hda-generic\;sof-hda-generic-ace1-2ch-48k-96k\;HDA_CONFIG=mix,PLATFORM=mtl,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-hda-generic-ace1-2ch-48k-96k.bin,\
DEEPBUFFER_FW_DMA_MS=false,HDA_ANALOG_CAPTURE_RATE=96000,HDA_ANALOG_PLAYBACK_RATE=96000,\
NUM_DMICS=2,PDM1_MIC_A_ENABLE=0,PDM1_MIC_B_ENABLE=0,DMIC1_ENABLE=passthrough,\
DMIC1_RATE=96000,DMIC1_PCM_ID=22"

# With 4ch 96 kHz DMIC0 and HDA
"sof-hda-generic\;sof-hda-generic-cavs25-4ch-96k\;HDA_CONFIG=mix,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-hda-generic-cavs25-4ch-96k.bin,\
DEEPBUFFER_FW_DMA_MS=false,HDA_ANALOG_CAPTURE_RATE=96000,HDA_ANALOG_PLAYBACK_RATE=96000,\
NUM_DMICS=4,PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,DMIC0_RATE=96000"

"sof-hda-generic\;sof-hda-generic-ace1-4ch-96k\;HDA_CONFIG=mix,PLATFORM=mtl,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-hda-generic-ace1-4ch-96k.bin,\
DEEPBUFFER_FW_DMA_MS=false,HDA_ANALOG_CAPTURE_RATE=96000,HDA_ANALOG_PLAYBACK_RATE=96000,\
NUM_DMICS=4,PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,DMIC0_RATE=96000"

# With 2ch 96 kHz DMIC0 and HDA
"sof-hda-generic\;sof-hda-generic-cavs25-2ch-96k\;HDA_CONFIG=mix,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-hda-generic-cavs25-2ch-96k.bin,\
DEEPBUFFER_FW_DMA_MS=false,HDA_ANALOG_CAPTURE_RATE=96000,HDA_ANALOG_PLAYBACK_RATE=96000,\
NUM_DMICS=2,PDM1_MIC_A_ENABLE=0,PDM1_MIC_B_ENABLE=0,DMIC0_RATE=96000"

"sof-hda-generic\;sof-hda-generic-ace1-2ch-96k\;HDA_CONFIG=mix,PLATFORM=mtl,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-hda-generic-ace1-2ch-96k.bin,\
DEEPBUFFER_FW_DMA_MS=false,HDA_ANALOG_CAPTURE_RATE=96000,HDA_ANALOG_PLAYBACK_RATE=96000,\
NUM_DMICS=2,PDM1_MIC_A_ENABLE=0,PDM1_MIC_B_ENABLE=0,DMIC0_RATE=96000"

# With 96 kHz HDA
"sof-hda-generic\;sof-hda-generic-96k\;HDA_CONFIG=mix,\
DEEPBUFFER_FW_DMA_MS=false,HDA_ANALOG_CAPTURE_RATE=96000,HDA_ANALOG_PLAYBACK_RATE=96000,NUM_DMICS=0"

# SDW + DMIC + HDMI, with 16 kHz DMIC1
"cavs-sdw\;sof-mtl-sdw-cs42l42-l0-max98363-l2-4ch-48k-16k\;PLATFORM=mtl,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-mtl-sdw-cs42l42-l0-max98363-l2-4ch-48k-16k.bin,\
NUM_DMICS=4,PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,DMIC0_ID=3,DMIC1_ID=4,\
DMIC1_ENABLE=passthrough,DMIC1_RATE=16000,\
BT_NAME=SSP1-BT,BT_INDEX=1,BT_PCM_ID=20,BT_ID=8,BT_PCM_NAME=Bluetooth,ADD_BT=true,\
NUM_SDW_AMP_LINKS=1,SDW_SPK_STREAM=SDW2-Playback,SDW_AMP_FEEDBACK=false,\
SDW_JACK_CAPTURE_CH=1,DEEPBUFFER_FW_DMA_MS=100,DEEPBUFFER_D0I3_COMPATIBLE=true"

# SDW + DMIC + HDMI, with 96 kHz DMIC1
"cavs-sdw\;sof-mtl-sdw-cs42l42-l0-max98363-l2-4ch-48k-96k\;PLATFORM=mtl,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-mtl-sdw-cs42l42-l0-max98363-l2-4ch-48k-96k.bin,\
NUM_DMICS=4,PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,DMIC0_ID=3,DMIC1_ID=4,\
DMIC1_ENABLE=passthrough,DMIC1_RATE=96000,DMIC1_PCM_ID=22,\
BT_NAME=SSP1-BT,BT_INDEX=1,BT_PCM_ID=20,BT_ID=8,BT_PCM_NAME=Bluetooth,ADD_BT=true,\
NUM_SDW_AMP_LINKS=1,SDW_SPK_STREAM=SDW2-Playback,SDW_AMP_FEEDBACK=false,\
SDW_JACK_CAPTURE_CH=1,DEEPBUFFER_FW_DMA_MS=100,DEEPBUFFER_D0I3_COMPATIBLE=true"

# SDW + DMIC + HDMI, with 96 kHz DMIC0
"cavs-sdw\;sof-mtl-sdw-cs42l42-l0-max98363-l2-4ch-96k\;PLATFORM=mtl,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-mtl-sdw-cs42l42-l0-max98363-l2-4ch-96k.bin,\
NUM_DMICS=4,PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,DMIC0_ID=3,DMIC1_ID=4,DMIC0_RATE=96000,\
BT_NAME=SSP1-BT,BT_INDEX=1,BT_PCM_ID=20,BT_ID=8,BT_PCM_NAME=Bluetooth,ADD_BT=true,\
NUM_SDW_AMP_LINKS=1,SDW_SPK_STREAM=SDW2-Playback,SDW_AMP_FEEDBACK=false,\
SDW_JACK_CAPTURE_CH=1,DEEPBUFFER_FW_DMA_MS=100,DEEPBUFFER_D0I3_COMPATIBLE=true"

# SDW + DMIC + HDMI, with 16 kHz DMIC1
"cavs-rt5682\;sof-mtl-max98360a-rt5682-4ch-48k-16k\;PLATFORM=mtl,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-mtl-max98360a-rt5682-4ch-48k-16k.bin,\
NUM_DMICS=4,PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,DMIC0_PCM_ID=99,\
DMIC1_ENABLE=passthrough,DMIC1_RATE=16000,DMIC1_PCM_ID=100,\
DEEPBUFFER_FW_DMA_MS=10,HEADSET_SSP_DAI_INDEX=2,\
SPK_ID=6,SPEAKER_SSP_DAI_INDEX=0,HEADSET_CODEC_NAME=SSP2-Codec,SPEAKER_CODEC_NAME=SSP0-Codec,\
BT_NAME=SSP1-BT,BT_INDEX=1,BT_ID=7,BT_PCM_NAME=Bluetooth,INCLUDE_ECHO_REF=true,\
GOOGLE_RTC_AEC_SUPPORT=1,DEEP_BUF_SPK=true,GOOGLE_AEC_DP_CORE_ID=2"

# SDW + DMIC + HDMI, with 96 kHz DMIC1
"cavs-rt5682\;sof-mtl-max98360a-rt5682-4ch-48k-96k\;PLATFORM=mtl,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-mtl-max98360a-rt5682-4ch-48k-96k.bin,\
NUM_DMICS=4,PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,DMIC0_PCM_ID=99,\
DMIC1_ENABLE=passthrough,DMIC1_RATE=96000,DMIC1_PCM_ID=100,\
DEEPBUFFER_FW_DMA_MS=10,HEADSET_SSP_DAI_INDEX=2,\
SPK_ID=6,SPEAKER_SSP_DAI_INDEX=0,HEADSET_CODEC_NAME=SSP2-Codec,SPEAKER_CODEC_NAME=SSP0-Codec,\
BT_NAME=SSP1-BT,BT_INDEX=1,BT_ID=7,BT_PCM_NAME=Bluetooth,INCLUDE_ECHO_REF=true,\
GOOGLE_RTC_AEC_SUPPORT=1,DEEP_BUF_SPK=true,GOOGLE_AEC_DP_CORE_ID=2"

# CAVS HDA topology with gain and SRC before mixin for HDA and passthrough pipelines for HDMI
"sof-hda-generic\;sof-hda-src-generic\;HDA_CONFIG=src"

# BT offload for tgl
"cavs-nocodec-bt\;sof-nocodec-bt-tgl\;PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-nocodec-bt-tgl.bin,\
PLATFORM=tgl"
# BT offload loopback test topology (lbm) for tgl
"cavs-nocodec-bt\;sof-nocodec-bt-tgl-lbm\;BT_LOOPBACK_MODE=true,PLATFORM=tgl,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-nocodec-bt-tgl-lbm.bin"

# BT offload for mtl
"cavs-nocodec-bt\;sof-nocodec-bt-mtl\;PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-nocodec-bt-mtl.bin,\
PLATFORM=mtl"
# BT offload loopback test topology (lbm) for mtl
"cavs-nocodec-bt\;sof-nocodec-bt-mtl-lbm\;BT_LOOPBACK_MODE=true,PLATFORM=mtl,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-nocodec-bt-mtl-lbm.bin"

# CAVS HDA topology for benchmarking performance
# Copier - peak volume - mixin - mixout - aria - peak volume - mixin - mixout - copier
"sof-hda-generic\;sof-hda-benchmark-generic\;HDA_CONFIG=benchmark,BENCH_CONFIG=benchmark"

# Topology to test IPC4 Crossover
"development/cavs-nocodec-crossover\;sof-tgl-nocodec-crossover-2way\;PLATFORM=tgl,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-tgl-nocodec-crossover.bin,EFX_CROSSOVER_PARAMS=2way"

# Topology to test RTC AEC
"development/cavs-nocodec-rtcaec\;sof-tgl-nocodec-rtcaec\;PLATFORM=tgl,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-tgl-nocodec-rtcaec.bin"

# Add MFCC to 16 kHz DMIC1, 2ch with NHLT - cAVS
"sof-hda-generic\;sof-hda-generic-cavs25-2ch-mfcc\;HDA_CONFIG=mix,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-hda-generic-cavs25-2ch-mfcc.bin,\
NUM_DMICS=2,DMIC1_RATE=16000,DMIC1_ENABLE=MFCC,DMIC1_MFCC_PARAMS=default"

# Add MFCC to 16 kHz DMIC1, 2ch with NHLT - ACE1
"sof-hda-generic\;sof-hda-generic-ace1-2ch-mfcc\;PLATFORM=mtl,HDA_CONFIG=mix,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-hda-generic-ace1-2ch-mfcc.bin,\
NUM_DMICS=2,DMIC1_RATE=16000,DMIC1_ENABLE=MFCC,DMIC1_MFCC_PARAMS=default"

# Add MFCC to 16 kHz DMIC1, 4ch with NHLT - cAVS
"sof-hda-generic\;sof-hda-generic-cavs25-4ch-mfcc\;HDA_CONFIG=mix,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-hda-generic-cavs25-4ch-mfcc.bin,\
NUM_DMICS=4,PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,DMIC1_RATE=16000,\
DMIC1_ENABLE=MFCC,DMIC1_MFCC_PARAMS=default"

# Add MFCC to 16 kHz DMIC1, 4ch with NHLT - ACE1
"sof-hda-generic\;sof-hda-generic-ace1-4ch-mfcc\;PLATFORM=mtl,HDA_CONFIG=mix,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-hda-generic-ace1-4ch-mfcc.bin,\
NUM_DMICS=4,PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,DMIC1_RATE=16000,\
DMIC1_ENABLE=MFCC,DMIC1_MFCC_PARAMS=default"

# Topology for SOF plugin
"development/sof-plugin\;sof-plugin\;NOISE_SUPPRESSION=false"

# mtl-rt5650 with AEC but not DTS
"cavs-rt5682\;sof-mtl-rt5650-google-aec\;PLATFORM=mtl,NUM_DMICS=4,PDM1_MIC_A_ENABLE=1,\
PDM1_MIC_B_ENABLE=1,DMIC0_PCM_ID=99,PREPROCESS_PLUGINS=nhlt,\
NHLT_BIN=nhlt-sof-mtl-rt5650-google-aec.bin,DEEPBUFFER_FW_DMA_MS=10,HEADSET_SSP_DAI_INDEX=2,\
SPK_ID=6,SPEAKER_SSP_DAI_INDEX=0,HEADSET_CODEC_NAME=SSP2-Codec,SPEAKER_CODEC_NAME=SSP0-Codec,\
BT_NAME=SSP1-BT,BT_INDEX=1,BT_ID=7,BT_PCM_NAME=Bluetooth,INCLUDE_ECHO_REF=true,\
GOOGLE_RTC_AEC_SUPPORT=1,DEEP_BUF_SPK=true,PLAYBACK_PIPELINE_SRC=volume,\
SSP_SPK_FMT_24=true,SSP_HS_FMT_24=true"

# mtl-rt1019 with CTC
"cavs-rt5682\;sof-mtl-rt1019-rt5682-ctc\;PLATFORM=mtl,NUM_DMICS=4,PDM1_MIC_A_ENABLE=1,\
PDM1_MIC_B_ENABLE=1,DMIC0_PCM_ID=99,SPK_ID=6,PREPROCESS_PLUGINS=nhlt,\
NHLT_BIN=nhlt-sof-max98357a-rt5682.bin,DEEPBUFFER_FW_DMA_MS=10,HEADSET_SSP_DAI_INDEX=2,\
SPEAKER_SSP_DAI_INDEX=0,HEADSET_CODEC_NAME=SSP2-Codec,SPEAKER_CODEC_NAME=SSP0-Codec,\
INCLUDE_ECHO_REF=true,INCLUDE_BT_OFFLOAD=false,PLAYBACK_PIPELINE_SRC=ctc"

# mtl-rt5650 with CTC
"cavs-rt5682\;sof-mtl-rt5650-ctc\;PLATFORM=mtl,NUM_DMICS=4,PDM1_MIC_A_ENABLE=1,\
PDM1_MIC_B_ENABLE=1,DMIC0_PCM_ID=99,PREPROCESS_PLUGINS=nhlt,\
NHLT_BIN=nhlt-sof-mtl-rt5650-dts.bin,DEEPBUFFER_FW_DMA_MS=10,HEADSET_SSP_DAI_INDEX=2,\
SPK_ID=6,SPEAKER_SSP_DAI_INDEX=0,HEADSET_CODEC_NAME=SSP2-Codec,SPEAKER_CODEC_NAME=SSP0-Codec,\
BT_NAME=SSP1-BT,BT_INDEX=1,BT_ID=7,BT_PCM_NAME=Bluetooth,INCLUDE_ECHO_REF=true,\
DEEP_BUF_SPK=true,PLAYBACK_PIPELINE_SRC=ctc,SSP_SPK_FMT_24=true,SSP_HS_FMT_24=true"
)
