#
# Topology for Icelake with rt711 + rt1308 (x2) + rt715.
#

# Include topology builder
include(`utils.m4')
include(`dai.m4')
include(`pipeline.m4')
include(`alh.m4')
include(`muxdemux.m4')
include(`hda.m4')

# Include TLV library
include(`common/tlv.m4')

# Include Token library
include(`sof/tokens.m4')

# Include Platform specific DSP configuration
include(`platform/intel/'PLATFORM`.m4')

ifdef(`UAJ_LINK',`',
`define(UAJ_LINK, `0')')

ifdef(`AMP_1_LINK',`',
`define(AMP_1_LINK, `1')')

ifdef(`AMP_2_LINK',`',
`define(AMP_2_LINK, `2')')

ifdef(`MIC_LINK',`',
`define(MIC_LINK, `3')')

# uncomment to remove HDMI support
#define(NOHDMI, `1')

# UAJ ID: 0, 1
# AMP ID: 2, 3 (if EXT_AMP_REF defined)
# DMIC ID: 4
# HDMI ID calculated based on the configuration
define(HDMI_BE_ID_BASE, `0')

ifdef(`NO_JACK', `',
	`undefine(`HDMI_BE_ID_BASE')
	 define(HDMI_BE_ID_BASE, `2')'
)

ifdef(`NOAMP', `',
	`undefine(`HDMI_BE_ID_BASE')
	 define(HDMI_BE_ID_BASE, `3')'
)

ifdef(`EXT_AMP_REF',
	`undefine(`HDMI_BE_ID_BASE')
	 define(HDMI_BE_ID_BASE, `4')',
	`'
)

ifdef(`NO_LOCAL_MIC', `',
	`undefine(`HDMI_BE_ID_BASE')
	 define(HDMI_BE_ID_BASE, `5')'
)

DEBUG_START

dnl Configure demux
dnl name, pipeline_id, routing_matrix_rows
dnl Diagonal 1's in routing matrix mean that every input channel is
dnl copied to corresponding output channels in all output streams.
dnl I.e. row index is the input channel, 1 means it is copied to
dnl corresponding output channel (column index), 0 means it is discarded.
dnl There's a separate matrix for all outputs.
ifdef(`NOAMP', `',
`
ifdef(`MONO', `',
`
define(matrix1, `ROUTE_MATRIX(3,
			     `BITS_TO_BYTE(1, 0, 0 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 1, 0 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 1 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,1 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,1 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,1 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,0 ,1 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,0 ,0 ,1)')')

define(matrix2, `ROUTE_MATRIX(4,
			     `BITS_TO_BYTE(1, 0, 0 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 1, 0 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 1 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,1 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,1 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,1 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,0 ,1 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,0 ,0 ,1)')')

dnl name, num_streams, route_matrix list
MUXDEMUX_CONFIG(demux_priv_3, 2, LIST_NONEWLINE(`', `matrix1,', `matrix2'))
')
')

#
# Define the pipelines
#
ifdef(`NOJACK', `',
`
# PCM0 ---> volume ----> mixer --->ALH 2 BE UAJ_LINK
# PCM31 ---> volume ------^
# PCM1 <--- volume <---- ALH 3 BE UAJ_LINK
')
ifdef(`NOAMP', `',
`
# PCM2 ---> volume ----> ALH 2 BE AMP_1_LINK
ifdef(`MONO', `',
`# PCM40 ---> volume ----> ALH 2 BE AMP_2_LINK')
')
ifdef(`NO_LOCAL_MIC', `',
`# PCM4 <--- volume <---- ALH 2 BE MIC_LINK')

ifdef(`NOHDMI', `',
`
# PCM5 ---> volume ----> iDisp1
# PCM6 ---> volume ----> iDisp2
# PCM7 ---> volume ----> iDisp3
')

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate,
dnl     time_domain, sched_comp)

ifdef(`NOJACK', `',
`
# Low Latency capture pipeline 2 on PCM 1 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-volume-switch-capture.m4,
	2, 1, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)
')

ifdef(`NOAMP', `',
`
ifdef(`MONO',
`
# Low Latency playback pipeline 3 on PCM 2 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	3, 2, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)
',
`
# Low Latency playback pipeline 3 on PCM 2 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-volume-demux-playback.m4,
	3, 2, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 4 on PCM 40 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-dai-endpoint.m4,
	4, 40, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)
')
')

ifdef(`NO_LOCAL_MIC', `',
`
# Low Latency capture pipeline 5 on PCM 4 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-highpass-switch-capture.m4,
	5, 4, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)
')

# Low Latency playback pipeline 6 on PCM 5 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	6, 5, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 7 on PCM 6 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	7, 6, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 8 on PCM 7 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline with priority 0 on core 0
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

ifdef(`NOJACK', `',
`
# playback DAI is ALH(UAJ_LINK PIN2) using 2 periods
# Buffers use s24le format, with 48 frame per 1000us on core 0 with priority 0
# The NOT_USED_IGNORED is due to dependencies and is adjusted later with an explicit dapm line.

DAI_ADD(sof/pipe-mixer-volume-dai-playback.m4,
	1, ALH, eval(UAJ_LINK * 256 + 2), `SDW'eval(UAJ_LINK)`-Playback',
	NOT_USE_IGNORED, 2, s24le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER, 2, 48000)

# capture DAI is ALH(UAJ_LINK PIN3) using 2 periods
# Buffers use s24le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	2, ALH, eval(UAJ_LINK * 256 + 3), `SDW'eval(UAJ_LINK)`-Capture',
	PIPELINE_SINK_2, 2, s24le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# Low Latency playback pipeline 30 on PCM 0 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-host-volume-playback.m4,
	30, 0, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000,
	SCHEDULE_TIME_DOMAIN_TIMER,
	PIPELINE_PLAYBACK_SCHED_COMP_1)

# Deep buffer playback pipeline 31 on PCM 31 using max 2 channels of s32le
# Set 1000us deadline on core 0 with priority 0.
# TODO: Modify pipeline deadline to account for deep buffering
ifdef(`HEADSET_DEEP_BUFFER',
`
PIPELINE_PCM_ADD(sof/pipe-host-volume-playback.m4,
	31, 31, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000,
	SCHEDULE_TIME_DOMAIN_TIMER,
	PIPELINE_PLAYBACK_SCHED_COMP_1)
'
)

SectionGraph."mixer-host" {
	index "0"

	lines [
		# connect mixer dai pipelines to PCM pipelines
		dapm(PIPELINE_MIXER_1, PIPELINE_SOURCE_30)
ifdef(`HEADSET_DEEP_BUFFER',
`
		dapm(PIPELINE_MIXER_1, PIPELINE_SOURCE_31)
'
)
	]
}

'
)

ifdef(`NOAMP', `',
`
# playback DAI is ALH(AMP_1_LINK PIN2/AMP_2_LINK PIN2) using 2 periods
# Buffers use s24le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	3, ALH, eval(AMP_1_LINK * 256 + 2), `SDW'eval(AMP_1_LINK)`-Playback',
	PIPELINE_SOURCE_3, 2, s24le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

ifdef(`MONO', `',
`DAI_ADD_SCHED(sof/pipe-dai-sched-playback.m4,
	4, ALH, eval(AMP_2_LINK * 256 + 2), `SDW'eval(AMP_1_LINK)`-Playback',
	PIPELINE_SOURCE_4, 2, s24le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER,
	PIPELINE_PLAYBACK_SCHED_COMP_3)

# Connect demux to 2nd pipeline
SectionGraph."PIPE_DEMUX" {
	index "4"

	lines [
		# mux to 2nd pipeline
		dapm(PIPELINE_SOURCE_4, PIPELINE_DEMUX_3)
	]
}
')
')

ifdef(`NO_LOCAL_MIC', `',
`
# capture DAI is ALH(MIC_LINK PIN2) using 2 periods
# Buffers use s24le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	5, ALH, eval(MIC_LINK * 256 + 2), `SDW'eval(MIC_LINK)`-Capture',
	PIPELINE_SINK_5, 2, s24le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)
')

ifdef(`NOHDMI', `',
`
# playback DAI is iDisp1 using 2 periods
# # Buffers use s32le format, 1000us deadline with priority 0 on core 0
DAI_ADD(sof/pipe-dai-playback.m4,
	6, HDA, 0, iDisp1,
	PIPELINE_SOURCE_6, 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is iDisp2 using 2 periods
# # Buffers use s32le format, 1000us deadline with priority 0 on core 0
DAI_ADD(sof/pipe-dai-playback.m4,
	7, HDA, 1, iDisp2,
	PIPELINE_SOURCE_7, 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is iDisp3 using 2 periods
# # Buffers use s32le format, 1000us deadline with priority 0 on core 0
DAI_ADD(sof/pipe-dai-playback.m4,
	8, HDA, 2, iDisp3,
	PIPELINE_SOURCE_8, 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)
')

# PCM Low Latency, id 0
dnl PCM_PLAYBACK_ADD(name, pcm_id, playback)

ifdef(`NOJACK', `',
`
PCM_PLAYBACK_ADD(Jack Out, 0, PIPELINE_PCM_30)
ifdef(`HEADSET_DEEP_BUFFER',
`
PCM_PLAYBACK_ADD(Jack Out DeepBuffer, 31, PIPELINE_PCM_31)
'
)
PCM_CAPTURE_ADD(Jack In, 1, PIPELINE_PCM_2)
')
ifdef(`NOAMP', `',
`
PCM_PLAYBACK_ADD(Speaker, 2, PIPELINE_PCM_3)
')
ifdef(`NO_LOCAL_MIC', `', `PCM_CAPTURE_ADD(Microphone, 4, PIPELINE_PCM_5)')

ifdef(`NOHDMI', `',
`
PCM_PLAYBACK_ADD(HDMI 1, 5, PIPELINE_PCM_6)
PCM_PLAYBACK_ADD(HDMI 2, 6, PIPELINE_PCM_7)
PCM_PLAYBACK_ADD(HDMI 3, 7, PIPELINE_PCM_8)
')

#
# BE configurations - overrides config in ACPI if present
#

ifdef(`NOJACK', `',
`
#ALH dai index = ((link_id << 8) | PDI id)
#ALH UAJ_LINK Pin2 (ID: 0)
DAI_CONFIG(ALH, eval(UAJ_LINK * 256 + 2), 0, `SDW'eval(UAJ_LINK)`-Playback',
	ALH_CONFIG(ALH_CONFIG_DATA(ALH, eval(UAJ_LINK * 256 + 2), 48000, 2)))

#ALH UAJ_LINK Pin3 (ID: 1)
DAI_CONFIG(ALH, eval(UAJ_LINK * 256 + 3), 1, `SDW'eval(UAJ_LINK)`-Capture',
	ALH_CONFIG(ALH_CONFIG_DATA(ALH, eval(UAJ_LINK * 256 + 3), 48000, 2)))
')

ifdef(`NOAMP', `',
`
#ALH AMP_1_LINK Pin2 (ID: 2)
DAI_CONFIG(ALH, eval(AMP_1_LINK * 256 + 2), 2, `SDW'eval(AMP_1_LINK)`-Playback',
	ALH_CONFIG(ALH_CONFIG_DATA(ALH, eval(AMP_1_LINK * 256 + 2), 48000, 2)))
')

ifdef(`NO_LOCAL_MIC', `',
`
#ALH MIC_LINK Pin2 (ID: 4)
DAI_CONFIG(ALH, eval(MIC_LINK * 256 + 2), 4, `SDW'eval(MIC_LINK)`-Capture',
	ALH_CONFIG(ALH_CONFIG_DATA(ALH, eval(MIC_LINK * 256 + 2), 48000, 2)))
')

ifdef(`NOHDMI', `',
`
# 3 HDMI/DP outputs (ID: from HDMI_BE_ID_BASE)
DAI_CONFIG(HDA, 0, HDMI_BE_ID_BASE, iDisp1,
	HDA_CONFIG(HDA_CONFIG_DATA(HDA, 0, 48000, 2)))
DAI_CONFIG(HDA, 1, eval(HDMI_BE_ID_BASE + 1), iDisp2,
	HDA_CONFIG(HDA_CONFIG_DATA(HDA, 1, 48000, 2)))
DAI_CONFIG(HDA, 2, eval(HDMI_BE_ID_BASE + 2), iDisp3,
	HDA_CONFIG(HDA_CONFIG_DATA(HDA, 2, 48000, 2)))
')

DEBUG_END
