#
# Topology for Tigerlake with sdw rt5682 + Max98373 amp + DMIC + 4 HDMI
#

# Include topology builder
include(`utils.m4')
include(`dai.m4')
include(`pipeline.m4')
include(`alh.m4')
include(`hda.m4')

# Include TLV library
include(`common/tlv.m4')

# Include Token library
include(`sof/tokens.m4')

# Include Tigerlake DSP configuration
include(`platform/intel/tgl.m4')
include(`platform/intel/dmic.m4')

define(DMIC_PDM_CONFIG, ifelse(CHANNELS, `4', ``FOUR_CH_PDM0_PDM1'',
	`ifelse(CHANNELS, `2', ``STEREO_PDM0'', `')'))

DEBUG_START

#
# Define the pipelines
#
# PCM0 <---> volume <----> playback (Headset - ALC5682)
# PCM1 <---> volume <----> capture  (Headset - ALC5682)
# PCM2 ----> smart_amp ----> ALH0x102 (Speaker -max98373)
#              ^
#              |
#              |
# PCM3 <---- demux  <----- ALH0x103 (Speaker -max98373)
# PCM4 <---- volume <---- DMIC01
# PCM5 <---- volume <---- DMIC16k
# PCM6 ----> volume -----> iDisp1
# PCM7 ----> volume -----> iDisp2
# PCM8 ----> volume -----> iDisp3
# PCM9 ----> volume -----> iDisp4

define(`SDW', 1)

# Smart amplifier related
# ALH related
#define smart amplifier ALH index
define(`SMART_ALH_INDEX', 0x102)
#define ALH BE dai_link name
define(`SMART_ALH_PLAYBACK_NAME', `SDW1-Playback')
define(`SMART_ALH_CAPTURE_NAME', `SDW1-Capture')
#define BE dai_link ID
define(`SMART_BE_ID', 2)

# Playback related
define(`SMART_PB_PPL_ID', 3)
define(`SMART_PB_CH_NUM', 2)
# channel number for playback on sdw link
define(`SMART_TX_CHANNELS', 2)
# channel number for I/V feedback on sdw link
define(`SMART_RX_CHANNELS', 4)
# Smart_amp algorithm specific. channel number
# for feedback provided to smart_amp algorithm
define(`SMART_FB_CHANNELS', 4)
# Ref capture related
define(`SMART_REF_PPL_ID', 4)
define(`SMART_REF_CH_NUM', 4)
# PCM related
define(`SMART_PCM_ID', 2)
define(`SMART_PCM_NAME', `smart373-spk')

# Include Smart Amplifier support
include(`sof-smart-amplifier.m4')

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     frames, deadline, priority, core)

# Low Latency playback pipeline 2 on PCM 1 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
        1, 0, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency capture pipeline 3 on PCM 1 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-capture.m4,
        2, 1, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Passthrough capture pipeline 4 on PCM 3 using max 4 channels.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-capture.m4,
        5, 4, 4, s32le,
        1000, 0, 0,
        48000, 48000, 48000)

# Passthrough capture pipeline 5 on PCM 4 using max 4 channels.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-capture-16khz.m4,
        6, 5, CHANNELS, s32le,
        1000, 0, 0,
        16000, 16000, 16000)

# Low Latency playback pipeline 5 on PCM 2 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	7, 6, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 6 on PCM 3 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	8, 7, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 7 on PCM 4 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	9, 8, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 8 on PCM 5 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	10, 9, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

#
# DAIs configuration
#

dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     frames, deadline, priority, core)

# playback DAI is ALH(ALH0 PIN2) using 2 periods
# Buffers use s24le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
        1, ALH, 2, SDW0-Playback,
        PIPELINE_SOURCE_1, 2, s24le,
        1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# capture DAI is ALH(ALH0 PIN2) using 2 periods
# Buffers use s24le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
        2, ALH, 3, SDW0-Capture,
        PIPELINE_SINK_2, 2, s24le,
        1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# capture DAI is DMIC01 using 2 periods
# Buffers use s32le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
        5, DMIC, 0, dmic01,
        PIPELINE_SINK_5, 2, s32le,
        1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# capture DAI is DMIC16k using 2 periods
# Buffers use s32le format, with 16 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
        6, DMIC, 1, dmic16k,
        PIPELINE_SINK_6, 2, s32le,
        1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is iDisp1 using 2 periods
# Buffers use s32le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	7, HDA, 0, iDisp1,
	PIPELINE_SOURCE_7, 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is iDisp2 using 2 periods
# Buffers use s32le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	8, HDA, 1, iDisp2,
	PIPELINE_SOURCE_8, 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is iDisp3 using 2 periods
# Buffers use s32le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	9, HDA, 2, iDisp3,
	PIPELINE_SOURCE_9, 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is iDisp4 using 2 periods
# Buffers use s32le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	10, HDA, 3, iDisp4,
	PIPELINE_SOURCE_10, 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

#
# Bind PCM with the pipeline
#
dnl PCM_PLAYBACK_ADD(name, pcm_id, playback)
PCM_PLAYBACK_ADD(Headphone, 0, PIPELINE_PCM_1)
PCM_CAPTURE_ADD(Headset mic, 1, PIPELINE_PCM_2)
PCM_CAPTURE_ADD(DMIC, 4, PIPELINE_PCM_5)
PCM_CAPTURE_ADD(DMIC16kHz, 5, PIPELINE_PCM_6)
PCM_PLAYBACK_ADD(HDMI1, 6, PIPELINE_PCM_7)
PCM_PLAYBACK_ADD(HDMI2, 7, PIPELINE_PCM_8)
PCM_PLAYBACK_ADD(HDMI3, 8, PIPELINE_PCM_9)
PCM_PLAYBACK_ADD(HDMI4, 9, PIPELINE_PCM_10)

#
# BE configurations - overrides config in ACPI if present
#
dnl DAI_CONFIG(type, dai_index, link_id, name, ssp_config/dmic_config)

#ALH SDW0 Pin2 (ID: 0)
DAI_CONFIG(ALH, 2, 0, SDW0-Playback,
        ALH_CONFIG(ALH_CONFIG_DATA(ALH, 2, 48000, 2)))

#ALH SDW0 Pin3 (ID: 1)
DAI_CONFIG(ALH, 3, 1, SDW0-Capture,
        ALH_CONFIG(ALH_CONFIG_DATA(ALH, 3, 48000, 2)))

# dmic01 (ID: 4)
DAI_CONFIG(DMIC, 0, 4, dmic01,
           DMIC_CONFIG(1, 500000, 4800000, 40, 60, 48000,
                DMIC_WORD_LENGTH(s32le), 400, DMIC, 0,
                PDM_CONFIG(DMIC, 0, FOUR_CH_PDM0_PDM1)))

# dmic16k (ID: 5)
DAI_CONFIG(DMIC, 1, 5, dmic16k,
           DMIC_CONFIG(1, 500000, 4800000, 40, 60, 16000,
                DMIC_WORD_LENGTH(s32le), 400, DMIC, 1,
                PDM_CONFIG(DMIC, 1, DMIC_PDM_CONFIG)))

# 4 HDMI/DP outputs (ID: 6,7,8,9)
DAI_CONFIG(HDA, 0, 6, iDisp1,
	HDA_CONFIG(HDA_CONFIG_DATA(HDA, 0, 48000, 2)))
DAI_CONFIG(HDA, 1, 7, iDisp2,
	HDA_CONFIG(HDA_CONFIG_DATA(HDA, 1, 48000, 2)))
DAI_CONFIG(HDA, 2, 8, iDisp3,
	HDA_CONFIG(HDA_CONFIG_DATA(HDA, 2, 48000, 2)))
DAI_CONFIG(HDA, 3, 9, iDisp4,
	HDA_CONFIG(HDA_CONFIG_DATA(HDA, 3, 48000, 2)))

DEBUG_END
