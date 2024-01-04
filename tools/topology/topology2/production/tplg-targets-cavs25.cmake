# SPDX-License-Identifier: BSD-3-Clause

# Array of "input-file-name;output-file-name;comma separated pre-processor variables"
list(APPEND TPLGS
# IPC4 topology for TGL rt711 Headset + rt1316 Amplifier + rt714 DMIC
"cavs-sdw\;sof-tgl-rt711-rt1316-rt714\;NUM_SDW_AMP_LINKS=2,SDW_DMIC=1,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-tgl-rt711-rt1316-rt714.bin"

"cavs-sdw\;sof-adl-rt711-l0-rt1316-l12-rt714-l3\;NUM_SDW_AMP_LINKS=2,SDW_DMIC=1"

"cavs-sdw\;sof-tgl-rt711-rt1308-rt715\;NUM_SDW_AMP_LINKS=2,SDW_DMIC=1,SDW_AMP_FEEDBACK=false"

# IPC4 topology for TGL rt711 Headset + rt1308 Amplifier + rt715 DMIC
"cavs-sdw\;sof-tgl-rt715-rt711-rt1308-mono\;NUM_SDW_AMP_LINKS=1,SDW_DMIC=1,\
SDW_JACK_OUT_STREAM=SDW1-Playback,SDW_JACK_IN_STREAM=SDW1-Capture,\
SDW_SPK_STREAM=SDW2-Playback,SDW_DMIC_STREAM=SDW0-Capture,SDW_AMP_FEEDBACK=false,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-tgl-rt715-rt711-rt1308-mono.bin"

# IPC4 topology for TGL rt711 Headset + rt1308 Amplifier + PCH DMIC
"cavs-sdw\;sof-tgl-rt711-rt1308-4ch\;NUM_SDW_AMP_LINKS=1,NUM_DMICS=4,DMIC0_ID=3,\
DMIC1_ID=4,PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,SDW_AMP_FEEDBACK=false,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-tgl-rt711-rt1308-4ch.bin"

"cavs-sdw\;sof-tgl-cs42l43-l3-cs35l56-l01\;PLATFORM=tgl,NUM_SDW_AMP_LINKS=2,SDW_DMIC=1,\
NUM_HDMIS=0,SDW_SPK_STREAM=Playback-SmartAmp,SDW_SPK_IN_STREAM=Capture-SmartAmp,\
SDW_DMIC_STREAM=Capture-SmartMic,SDW_JACK_OUT_STREAM=Playback-SimpleJack,\
SDW_JACK_IN_STREAM=Capture-SimpleJack,SDW_AMP_FMT_24=true,SDW_JACK_FMT_24=true"

# IPC4 topology for TGL rt712 Headset, Amp and DMIC
"cavs-sdw\;sof-tgl-rt712\;SDW_JACK_OUT_STREAM=SDW0-Playback-SimpleJack,\
SDW_JACK_IN_STREAM=SDW0-Capture-SimpleJack,SDW_SPK_STREAM=SDW0-Playback-SmartAmp,\
SDW_DMIC_STREAM=SDW1-Capture-SmartMic,HDMI1_ID=5,HDMI2_ID=6,HDMI3_ID=7,\
NUM_SDW_AMP_LINKS=1,SDW_AMP_FEEDBACK=false,SDW_DMIC=1,SDW_DMIC_STREAM=SDW1-Capture-SmartMic"

"cavs-sdw\;sof-adl-rt711-4ch\;NUM_DMICS=4,DMIC0_ID=2,DMIC1_ID=3,\
PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,HDMI1_ID=4,HDMI2_ID=5,\
HDMI3_ID=6,PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-adl-rt711-4ch.bin"
)
