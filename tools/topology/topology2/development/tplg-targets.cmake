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
"cavs-nocodec\;sof-tgl-nocodec\;NUM_DMICS=4,PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-tgl-nocodec.bin,\
SSP0_MIXER_2LEVEL=1,PLATFORM=tgl"

"cavs-nocodec\;sof-adl-nocodec\;NUM_DMICS=4,PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,\
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

# CAVS HDA topology with mixer-based efx eq pipelines for HDA and passthrough pipelines for HDMI
"sof-hda-generic\;sof-hda-efx-generic\;HDA_CONFIG=efx,\
EFX_FIR_PARAMS=passthrough,EFX_IIR_PARAMS=passthrough,EFX_DRC_PARAMS=passthrough"

"sof-hda-generic\;sof-hda-efx-generic-2ch\;\
HDA_CONFIG=efx,NUM_DMICS=2,PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-hda-fir-generic-2ch.bin,\
EFX_FIR_PARAMS=passthrough,EFX_IIR_PARAMS=passthrough,\
EFX_DRC_PARAMS=passthrough"

"sof-hda-generic\;sof-hda-efx-generic-4ch\;\
HDA_CONFIG=efx,NUM_DMICS=4,PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-hda-efx-generic-4ch.bin,\
EFX_FIR_PARAMS=passthrough,EFX_IIR_PARAMS=passthrough,\
EFX_DRC_PARAMS=passthrough"

"sof-hda-generic\;sof-hda-efx-mbdrc-generic\;HDA_CONFIG=efx,\
EFX_FIR_PARAMS=passthrough,EFX_IIR_PARAMS=passthrough,\
EFX_DRC_COMPONENT=multiband,EFX_MBDRC_PARAMS=passthrough"

"sof-hda-generic\;sof-hda-efx-mbdrc-generic-2ch\;\
HDA_CONFIG=efx,NUM_DMICS=2,PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-hda-fir-generic-2ch.bin,\
EFX_FIR_PARAMS=passthrough,EFX_IIR_PARAMS=passthrough,\
EFX_DRC_COMPONENT=multiband,EFX_MBDRC_PARAMS=passthrough"

"sof-hda-generic\;sof-hda-efx-mbdrc-generic-4ch\;\
HDA_CONFIG=efx,NUM_DMICS=4,PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-hda-efx-generic-4ch.bin,\
EFX_FIR_PARAMS=passthrough,EFX_IIR_PARAMS=passthrough,\
EFX_DRC_COMPONENT=multiband,EFX_MBDRC_PARAMS=passthrough"

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
"sof-hda-generic\;sof-hda-benchmark-generic-tgl\;PLATFORM=TGL,HDA_CONFIG=benchmark,\
BENCH_CONFIG=benchmark"
"sof-hda-generic\;sof-hda-benchmark-generic-mtl\;PLATFORM=MTL,HDA_CONFIG=benchmark,\
BENCH_CONFIG=benchmark"
"sof-hda-generic\;sof-hda-benchmark-generic-lnl\;PLATFORM=LNL,HDA_CONFIG=benchmark,\
BENCH_CONFIG=benchmark"

# Topology to test IPC4 Crossover
"development/cavs-nocodec-crossover\;sof-tgl-nocodec-crossover-2way\;PLATFORM=tgl,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-tgl-nocodec-crossover.bin,EFX_CROSSOVER_PARAMS=2way"

# Topology to test RTC AEC
"development/cavs-nocodec-rtcaec\;sof-tgl-nocodec-rtcaec\;PLATFORM=tgl,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-tgl-nocodec-rtcaec.bin"
)
