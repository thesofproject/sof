# SPDX-License-Identifier: BSD-3-Clause

# Array of "input-file-name;output-file-name;comma separated pre-processor variables"
list(APPEND TPLGS

"cavs-sdw\;sof-sdw-generic\;SDW_DMIC=1,NUM_SDW_AMP_LINKS=1,\
SDW_AMP_FEEDBACK=false,SDW_SPK_STREAM=Playback-SmartAmp,SDW_DMIC_STREAM=Capture-SmartMic,\
SDW_JACK_OUT_STREAM=Playback-SimpleJack,SDW_JACK_IN_STREAM=Capture-SimpleJack"

# Split topologies
"cavs-sdw\;sof-sdca-jack-id0\;SDW_JACK_OUT_STREAM=Playback-SimpleJack,\
SDW_JACK_IN_STREAM=Capture-SimpleJack,NUM_HDMIS=0"

"cavs-sdw\;sof-sdca-1amp-id2\;NUM_SDW_AMP_LINKS=1,SDW_JACK=false,\
SDW_AMP_FEEDBACK=false,SDW_SPK_STREAM=Playback-SmartAmp,NUM_HDMIS=0"

"cavs-sdw\;sof-sdca-2amp-id2\;NUM_SDW_AMP_LINKS=2,SDW_JACK=false,\
SDW_AMP_FEEDBACK=false,SDW_SPK_STREAM=Playback-SmartAmp,NUM_HDMIS=0"

"cavs-sdw\;sof-sdca-mic-id4\;SDW_JACK=false,SDW_DMIC=1,NUM_HDMIS=0,\
SDW_DMIC_STREAM=Capture-SmartMic"

"cavs-sdw\;sof-hdmi-pcm5-id4\;SDW_JACK=false,HDMI1_ID=4,HDMI2_ID=5,HDMI3_ID=6"
"cavs-sdw\;sof-hdmi-pcm5-id5\;SDW_JACK=false"
"cavs-sdw\;sof-hdmi-pcm5-id6\;SDW_JACK=false,HDMI1_ID=6,HDMI2_ID=7,HDMI3_ID=8"
"cavs-sdw\;sof-hdmi-pcm5-id7\;SDW_JACK=false,HDMI1_ID=7,HDMI2_ID=8,HDMI3_ID=9"

)
