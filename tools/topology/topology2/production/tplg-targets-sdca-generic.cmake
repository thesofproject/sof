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

# Topology for speaker with 2-way crossover filter in SOF
# with channels order L-low, R-low, L-high, R-high
"cavs-sdw\;sof-sdca-2amp-id2-xover\;NUM_SDW_AMP_LINKS=2,SDW_JACK=false,\
SDW_AMP_FEEDBACK=false,SDW_SPK_STREAM=Playback-SmartAmp,NUM_HDMIS=0,\
SDW_AMP_NUM_CHANNELS=4,SDW_AMP_XOVER=true,\
SDW_AMP_XOVER_SELECTOR_PARAMS=xover_selector_lr_to_lrlr,\
SDW_AMP_XOVER_EQIIR_PARAMS=xover_lr4_2000hz_llhh_48khz"

"cavs-sdw\;sof-sdca-2amp-feedback-id3-xover\;NUM_SDW_AMP_LINKS=2,\
SDW_JACK=false,SDW_AMP_FEEDBACK=true,SDW_SPK_STREAM=Playback-SmartAmp,\
SDW_SPK_IN_STREAM=Capture-SmartAmp,NUM_HDMIS=0,\
SDW_AMP_NUM_CHANNELS=4,SDW_AMP_XOVER=true,\
SDW_AMP_XOVER_SELECTOR_PARAMS=xover_selector_lr_to_lrlr,\
SDW_AMP_XOVER_EQIIR_PARAMS=xover_lr4_2000hz_llhh_48khz"
)
