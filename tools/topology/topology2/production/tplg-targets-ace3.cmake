# SPDX-License-Identifier: BSD-3-Clause

# Array of "input-file-name;output-file-name;comma separated pre-processor variables"
list(APPEND TPLGS
# SDW topology for PTL RVP
# RT721 eval board with SDW-DMIC, sof_sdw_quirk_table without SOC_SDW_PCH_DMIC
"cavs-sdw\;sof-ptl-rt721\;PLATFORM=ptl,SDW_DMIC=1,NUM_SDW_AMP_LINKS=1,\
SDW_AMP_FEEDBACK=false,SDW_SPK_STREAM=Playback-SmartAmp,SDW_DMIC_STREAM=Capture-SmartMic,\
SDW_JACK_OUT_STREAM=Playback-SimpleJack,SDW_JACK_IN_STREAM=Capture-SimpleJack"

# RT721 eval board with PCH-DMIC, sof_sdw_quirk_table with SOC_SDW_PCH_DMIC
"cavs-sdw\;sof-ptl-rt721-4ch\;PLATFORM=ptl,SDW_DMIC=1,NUM_SDW_AMP_LINKS=1,NUM_DMICS=4,\
PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,DMIC0_ID=5,DMIC1_ID=6,HDMI1_ID=7,HDMI2_ID=8,HDMI3_ID=9,\
SDW_AMP_FEEDBACK=false,SDW_SPK_STREAM=Playback-SmartAmp,SDW_DMIC_STREAM=Capture-SmartMic,\
SDW_JACK_OUT_STREAM=Playback-SimpleJack,SDW_JACK_IN_STREAM=Capture-SimpleJack,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-ptl-rt721-4ch.bin"

"cavs-sdw\;sof-ptl-rt722\;PLATFORM=ptl,SDW_DMIC=1,NUM_SDW_AMP_LINKS=1,\
SDW_AMP_FEEDBACK=false,SDW_SPK_STREAM=Playback-SmartAmp,SDW_DMIC_STREAM=Capture-SmartMic,\
SDW_JACK_OUT_STREAM=Playback-SimpleJack,SDW_JACK_IN_STREAM=Capture-SimpleJack"

"cavs-sdw\;sof-ptl-rt722-96k\;PLATFORM=ptl,SDW_DMIC=1,NUM_SDW_AMP_LINKS=1,\
SDW_AMP_FEEDBACK=false,SDW_SPK_STREAM=Playback-SmartAmp,SDW_DMIC_STREAM=Capture-SmartMic,\
SDW_JACK_OUT_STREAM=Playback-SimpleJack,SDW_JACK_IN_STREAM=Capture-SimpleJack,\
JACK_RATE=96000,DEEP_BUF_JACK_RATE=96000,\
SDW_SPK_ENHANCED_PLAYBACK=false,SDW_DMIC_ENHANCED_CAPTURE=false"

"cavs-sdw\;sof-ptl-rt722-192k\;PLATFORM=ptl,SDW_DMIC=1,NUM_SDW_AMP_LINKS=1,\
SDW_AMP_FEEDBACK=false,SDW_SPK_STREAM=Playback-SmartAmp,SDW_DMIC_STREAM=Capture-SmartMic,\
SDW_JACK_OUT_STREAM=Playback-SimpleJack,SDW_JACK_IN_STREAM=Capture-SimpleJack,\
JACK_RATE=192000,DEEP_BUF_JACK_RATE=192000,\
SDW_SPK_ENHANCED_PLAYBACK=false,SDW_DMIC_ENHANCED_CAPTURE=false"

"cavs-sdw\;sof-ptl-rt722-4ch\;PLATFORM=ptl,SDW_DMIC=1,NUM_SDW_AMP_LINKS=1,NUM_DMICS=4,\
PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,DMIC0_ID=5,DMIC1_ID=6,HDMI1_ID=7,HDMI2_ID=8,HDMI3_ID=9,\
SDW_AMP_FEEDBACK=false,SDW_SPK_STREAM=Playback-SmartAmp,SDW_DMIC_STREAM=Capture-SmartMic,\
SDW_JACK_OUT_STREAM=Playback-SimpleJack,SDW_JACK_IN_STREAM=Capture-SimpleJack,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-ptl-rt722-4ch.bin,DMIC0_ENHANCED_CAPTURE=true,\
EFX_DMIC0_TDFB_PARAMS=line4_pass,EFX_DMIC0_DRC_PARAMS=dmic_default,\
BT_NAME=SSP2-BT,BT_PCM_ID=20,BT_ID=10,BT_PCM_NAME=Bluetooth,ADD_BT=true,\
DEEPBUFFER_FW_DMA_MS=10,DEEP_BUF_SPK=true"

"cavs-sdw\;sof-ptl-rt712-l2-rt1320-l1\;PLATFORM=ptl,SDW_DMIC=1,NUM_SDW_AMP_LINKS=2,\
SDW_AMP_FEEDBACK=false,SDW_SPK_STREAM=Playback-SmartAmp,SDW_DMIC_STREAM=Capture-SmartMic,\
SDW_JACK_OUT_STREAM=Playback-SimpleJack,SDW_JACK_IN_STREAM=Capture-SimpleJack"

"cavs-sdw\;sof-ptl-rt712-l3-rt1320-l2\;PLATFORM=ptl,SDW_DMIC=1,NUM_SDW_AMP_LINKS=2,\
SDW_AMP_FEEDBACK=false,SDW_SPK_STREAM=Playback-SmartAmp,SDW_DMIC_STREAM=Capture-SmartMic,\
SDW_JACK_OUT_STREAM=Playback-SimpleJack,SDW_JACK_IN_STREAM=Capture-SimpleJack"

"cavs-sdw\;sof-ptl-rt713-l2-rt1320-l13\;PLATFORM=ptl,SDW_DMIC=1,NUM_SDW_AMP_LINKS=2,\
SDW_AMP_FEEDBACK=false,SDW_SPK_STREAM=Playback-SmartAmp,SDW_DMIC_STREAM=Capture-SmartMic,\
SDW_JACK_OUT_STREAM=Playback-SimpleJack,SDW_JACK_IN_STREAM=Capture-SimpleJack"

"cavs-sdw\;sof-ptl-rt713-l3-rt1320-l12\;PLATFORM=ptl,SDW_DMIC=1,NUM_SDW_AMP_LINKS=2,\
SDW_AMP_FEEDBACK=false,SDW_SPK_STREAM=Playback-SmartAmp,SDW_DMIC_STREAM=Capture-SmartMic,\
SDW_JACK_OUT_STREAM=Playback-SimpleJack,SDW_JACK_IN_STREAM=Capture-SimpleJack"
)
