# SPDX-License-Identifier: BSD-3-Clause

# Array of "input-file-name;output-file-name;comma separated pre-processor variables"
list(APPEND TPLGS
# SDW topology for LNL RVP
"cavs-sdw\;sof-lnl-rt711-4ch\;PLATFORM=lnl,NUM_DMICS=4,PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,\
DMIC0_ID=2,DMIC1_ID=3,NUM_HDMIS=0,PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-lnl-rt711-4ch.bin"

"cavs-sdw\;sof-lnl-rt711-l0-rt1316-l23-rt714-l1\;PLATFORM=lnl,NUM_SDW_AMP_LINKS=2,SDW_DMIC=1,\
NUM_HDMIS=0,SDW_SPK_STREAM=SDW2-Playback,SDW_SPK_IN_STREAM=SDW2-Capture,SDW_DMIC_STREAM=SDW1-Capture"

"cavs-sdw\;sof-lnl-rt712-l2-rt1712-l3\;PLATFORM=lnl,SDW_DMIC=1,NUM_HDMIS=0,NUM_SDW_AMP_LINKS=1,\
SDW_AMP_FEEDBACK=false,SDW_SPK_STREAM=Playback-SmartAmp,SDW_DMIC_STREAM=Capture-SmartMic,\
SDW_JACK_OUT_STREAM=Playback-SimpleJack,SDW_JACK_IN_STREAM=Capture-SimpleJack"

"cavs-sdw\;sof-lnl-rt722-l0\;PLATFORM=lnl,SDW_DMIC=1,NUM_HDMIS=0,NUM_SDW_AMP_LINKS=1,\
SDW_AMP_FEEDBACK=false,SDW_SPK_STREAM=Playback-SmartAmp,SDW_DMIC_STREAM=Capture-SmartMic,\
SDW_JACK_OUT_STREAM=Playback-SimpleJack,SDW_JACK_IN_STREAM=Capture-SimpleJack"

)
