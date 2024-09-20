# SPDX-License-Identifier: BSD-3-Clause

# Array of "input-file-name;output-file-name;comma separated pre-processor variables"
list(APPEND TPLGS
# SDW topology for PTL RVP
"cavs-sdw\;sof-ptl-rt722\;PLATFORM=ptl,SDW_DMIC=1,NUM_SDW_AMP_LINKS=1,\
SDW_AMP_FEEDBACK=false,SDW_SPK_STREAM=Playback-SmartAmp,SDW_DMIC_STREAM=Capture-SmartMic,\
SDW_JACK_OUT_STREAM=Playback-SimpleJack,SDW_JACK_IN_STREAM=Capture-SimpleJack"

"cavs-sdw\;sof-ptl-rt722-4ch\;PLATFORM=ptl,SDW_DMIC=1,NUM_SDW_AMP_LINKS=1,NUM_DMICS=4,\
PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,DMIC0_ID=5,DMIC1_ID=6,HDMI1_ID=7,HDMI2_ID=8,HDMI3_ID=9,\
SDW_AMP_FEEDBACK=false,SDW_SPK_STREAM=Playback-SmartAmp,SDW_DMIC_STREAM=Capture-SmartMic,\
SDW_JACK_OUT_STREAM=Playback-SimpleJack,SDW_JACK_IN_STREAM=Capture-SimpleJack,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-ptl-rt722-4ch.bin"

)
