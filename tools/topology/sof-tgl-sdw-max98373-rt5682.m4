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
# PCM5 ----> volume -----> iDisp1
# PCM6 ----> volume -----> iDisp2
# PCM7 ----> volume -----> iDisp3
# PCM9 ----> volume -----> iDisp4
# PCM10 <---- volume <---- DMIC01  (dmic 48k capture)
# PCM12 <---- kpb <---- DMIC16k  (dmic 16k capture)


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
define(`SMART_REF_CH_NUM', 2)
# PCM related
define(`SMART_PCM_ID', 2)
define(`SMART_PCM_NAME', `Speaker')

# Include Smart Amplifier support
include(`sof-smart-amplifier.m4')

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate,
dnl     time_domain, sched_comp)

# Low Latency playback pipeline 1 on PCM 0 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
        1, 0, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency capture pipeline 2 on PCM 1 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-capture.m4,
        2, 1, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Define pipeline id for intel-generic-dmic-kwd.m4
# to generate dmic setting with kwd when we have dmic
# define channel
define(CHANNELS, `4')
# define kfbm with volume
define(KFBM_TYPE, `vol-kfbm')
# define pcm, pipeline and dai id
define(DMIC_PCM_48k_ID, `10')
define(DMIC_PIPELINE_48k_ID, `11')
define(DMIC_DAI_LINK_48k_ID, `4')
define(DMIC_PCM_16k_ID, `12')
define(DMIC_PIPELINE_16k_ID, `12')
define(DMIC_PIPELINE_KWD_ID, `13')
define(DMIC_DAI_LINK_16k_ID, `5')
define(DMIC_16k_PCM_NAME, `BufferedMic')
# define pcm, pipeline and dai id
define(KWD_PIPE_SCH_DEADLINE_US, 5000)
# include the generic dmic with kwd
include(`platform/intel/intel-generic-dmic-kwd.m4')


# Low Latency playback pipeline 6 on PCM 5 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	6, 5, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 7 on PCM 6 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	7, 6, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 8 on PCM 7 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	8, 7, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 9 on PCM 8 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	9, 8, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

#
# DAIs configuration
#

dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     deadline, priority, core, time_domain)

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

# playback DAI is iDisp1 using 2 periods
# Buffers use s32le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	6, HDA, 0, iDisp1,
	PIPELINE_SOURCE_6, 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is iDisp2 using 2 periods
# Buffers use s32le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	7, HDA, 1, iDisp2,
	PIPELINE_SOURCE_7, 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is iDisp3 using 2 periods
# Buffers use s32le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	8, HDA, 2, iDisp3,
	PIPELINE_SOURCE_8, 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is iDisp4 using 2 periods
# Buffers use s32le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	9, HDA, 3, iDisp4,
	PIPELINE_SOURCE_9, 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

#
# Bind PCM with the pipeline
#
dnl PCM_PLAYBACK_ADD(name, pcm_id, playback)
PCM_PLAYBACK_ADD(Jack Out, 0, PIPELINE_PCM_1)
PCM_CAPTURE_ADD(Jack In, 1, PIPELINE_PCM_2)
PCM_PLAYBACK_ADD(HDMI 1, 5, PIPELINE_PCM_6)
PCM_PLAYBACK_ADD(HDMI 2, 6, PIPELINE_PCM_7)
PCM_PLAYBACK_ADD(HDMI 3, 7, PIPELINE_PCM_8)
PCM_PLAYBACK_ADD(HDMI 4, 8, PIPELINE_PCM_9)

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
