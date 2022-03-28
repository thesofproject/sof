#
# Topology for Icelake with rt700 codec.
#

# if XPROC is not defined, define with default pipe
ifdef(`DMICPROC', , `define(DMICPROC, eq-iir-volume)')
ifdef(`DMIC16KPROC', , `define(DMIC16KPROC, eq-iir-volume)')

# Include topology builder
include(`utils.m4')
include(`dai.m4')
include(`pipeline.m4')
include(`alh.m4')
include(`hda.m4')
include(`platform/intel/dmic.m4')

# Include TLV library
include(`common/tlv.m4')

# Include Token library
include(`sof/tokens.m4')

# Include Icelake DSP configuration
include(`platform/intel/icl.m4')

# Define pipeline id for intel-generic-dmic.m4
# to generate dmic setting

ifelse(CHANNELS, `0', ,
`
define(DMIC_PCM_48k_ID, `10')
define(DMIC_PIPELINE_48k_ID, `3')
define(DMIC_DAI_LINK_48k_ID, `2')

define(DMIC_PCM_16k_ID, `11')
define(DMIC_PIPELINE_16k_ID, `4')
define(DMIC_DAI_LINK_16k_ID, `3')

include(`platform/intel/intel-generic-dmic.m4')
'
)

DEBUG_START

#
# Define the pipelines
#
# PCM0 ---> volume ----> ALH 2 BE dailink 0
# PCM1 <--- volume <---- ALH 3 BE dailink 1
# PCM10 <---------------- DMIC01 (dmic0 capture, BE dailink 2)
# PCM11 <---------------- DMIC16k (dmic16k, BE dailink 3)
# PCM5 ----> volume -----> iDisp1 (HDMI/DP playback, BE link 4)
# PCM6 ----> volume -----> iDisp2 (HDMI/DP playback, BE link 5)
# PCM7 ----> volume -----> iDisp3 (HDMI/DP playback, BE link 6)
#

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate,
dnl     time_domain, sched_comp)

# Low Latency playback pipeline 1 on PCM 0 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	1, 0, 2, s24le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency capture pipeline 2 on PCM 1 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-capture.m4,
	2, 1, 2, s24le,
	1000, 0, 0,
	48000, 48000, 48000)

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

#
# DAIs configuration
#

dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     deadline, priority, core, time_domain)

# playback DAI is ALH(SDW0 PIN2) using 2 periods
# Buffers use s32le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	1, ALH, 2, SDW0-Playback,
	PIPELINE_SOURCE_1, 2, s24le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# capture DAI is ALH(SDW0 PIN2) using 2 periods
# Buffers use s32le format, with 48 frame per 1000us on core 0 with priority 0
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

# PCM Low Latency, id 0
dnl PCM_PLAYBACK_ADD(name, pcm_id, playback)
PCM_PLAYBACK_ADD(Jack Out, 0, PIPELINE_PCM_1)
PCM_CAPTURE_ADD(Jack In, 1, PIPELINE_PCM_2)
PCM_PLAYBACK_ADD(HDMI 1, 5, PIPELINE_PCM_6)
PCM_PLAYBACK_ADD(HDMI 2, 6, PIPELINE_PCM_7)
PCM_PLAYBACK_ADD(HDMI 3, 7, PIPELINE_PCM_8)

#
# BE configurations - overrides config in ACPI if present
#

#ALH dai index = ((link_id << 8) | PDI id)
#ALH SDW0 Pin2 (ID: 0)
DAI_CONFIG(ALH, 2, 0, SDW0-Playback,
	ALH_CONFIG(ALH_CONFIG_DATA(ALH, 2, 48000, 2)))

#ALH SDW0 Pin3 (ID: 1)
DAI_CONFIG(ALH, 3, 1, SDW0-Capture,
	ALH_CONFIG(ALH_CONFIG_DATA(ALH, 3, 48000, 2)))

# 3 HDMI/DP outputs (ID: 4,5,6)
DAI_CONFIG(HDA, 0, 4, iDisp1,
	HDA_CONFIG(HDA_CONFIG_DATA(HDA, 0, 48000, 2)))
DAI_CONFIG(HDA, 1, 5, iDisp2,
	HDA_CONFIG(HDA_CONFIG_DATA(HDA, 1, 48000, 2)))
DAI_CONFIG(HDA, 2, 6, iDisp3,
	HDA_CONFIG(HDA_CONFIG_DATA(HDA, 2, 48000, 2)))

DEBUG_END
