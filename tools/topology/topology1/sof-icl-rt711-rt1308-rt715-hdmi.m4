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

DEBUG_START

dnl Configure demux
dnl name, pipeline_id, routing_matrix_rows
dnl Diagonal 1's in routing matrix mean that every input channel is
dnl copied to corresponding output channels in all output streams.
dnl I.e. row index is the input channel, 1 means it is copied to
dnl corresponding output channel (column index), 0 means it is discarded.
dnl There's a separate matrix for all outputs.
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
MUXDEMUX_CONFIG(demux_priv_3, 2, LIST(`	', `matrix1,', `matrix2'))
')

#
# Define the pipelines
#
ifdef(`NOJACK', `',
`
# PCM0 ---> volume ----> ALH 2 BE dailink 0
# PCM1 <--- volume <---- ALH 3 BE dailink 1
')
# PCM2 ---> volume ----> ALH 2 BE dailink 2
ifdef(`MONO', `',
`# PCM40 ---> volume ----> ALH 2 BE dailink 3')
# PCM4 <--- volume <---- ALH 2 BE dailink 4
# PCM5 ---> volume ----> iDisp1
# PCM6 ---> volume ----> iDisp2
# PCM7 ---> volume ----> iDisp3

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate,
dnl     time_domain, sched_comp)

ifdef(`NOJACK', `',
`
# Low Latency playback pipeline 1 on PCM 0 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	1, 0, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency capture pipeline 2 on PCM 1 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-switch-capture.m4,
	2, 1, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)
')

ifdef(`MONO',
`
# Low Latency playback pipeline 3 on PCM 2 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	3, 2, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)
',
`
# Low Latency playback pipeline 3 on PCM 2 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(ifdef(`NO_AGGREGATION',`sof/pipe-volume-playback.m4',
	`sof/pipe-volume-demux-playback.m4'),
	3, 2, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 4 on PCM 40 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(ifdef(`NO_AGGREGATION', `sof/pipe-volume-playback.m4',
	`sof/pipe-dai-endpoint.m4'),
	4, 40, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)
')

# Low Latency capture pipeline 5 on PCM 4 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-highpass-switch-capture.m4,
	5, 4, 2, s32le,
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

ifdef(`NOJACK', `',
`
# playback DAI is ALH(SDW0 PIN2) using 2 periods
# Buffers use s24le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	1, ALH, eval(UAJ_LINK * 256 + 2), `SDW'eval(UAJ_LINK)`-Playback',
	PIPELINE_SOURCE_1, 2, s24le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# capture DAI is ALH(SDW0 PIN2) using 2 periods
# Buffers use s24le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	2, ALH, eval(UAJ_LINK * 256 + 3), `SDW'eval(UAJ_LINK)`-Capture',
	PIPELINE_SINK_2, 2, s24le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)
')

# playback DAI is ALH(SDW1 PIN2) using 2 periods
# Buffers use s24le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	3, ALH, eval(AMP_1_LINK * 256 + 2), `SDW'eval(AMP_1_LINK)`-Playback',
	PIPELINE_SOURCE_3, 2, s24le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

ifdef(`NO_AGGREGATION',
`# playback DAI is ALH(SDW2 PIN2) using 2 periods
# Buffers use s24le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	4, ALH, eval(AMP_2_LINK * 256 + 2), `SDW'eval(AMP_2_LINK)`-Playback',
	PIPELINE_SOURCE_4, 2, s24le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)',
`ifdef(`MONO', `',
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
')')

# capture DAI is ALH(SDW3 PIN2) using 2 periods
# Buffers use s24le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	5, ALH, eval(MIC_LINK * 256 + 2), `SDW'eval(MIC_LINK)`-Capture',
	PIPELINE_SINK_5, 2, s24le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is iDisp1 using 2 periods
# # Buffers use s32le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	6, HDA, 0, iDisp1,
	PIPELINE_SOURCE_6, 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is iDisp2 using 2 periods
# # Buffers use s32le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	7, HDA, 1, iDisp2,
	PIPELINE_SOURCE_7, 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is iDisp3 using 2 periods
# # Buffers use s32le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	8, HDA, 2, iDisp3,
	PIPELINE_SOURCE_8, 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# PCM Low Latency, id 0
dnl PCM_PLAYBACK_ADD(name, pcm_id, playback)

ifdef(`NOJACK', `',
`
PCM_PLAYBACK_ADD(Jack Out, 0, PIPELINE_PCM_1)
PCM_CAPTURE_ADD(Jack In, 1, PIPELINE_PCM_2)
')

PCM_PLAYBACK_ADD(Speaker, 2, PIPELINE_PCM_3)
ifdef(`NO_AGGREGATION', `PCM_PLAYBACK_ADD(Speaker 2, 40, PIPELINE_PCM_4)',`')
PCM_CAPTURE_ADD(Microphone, 4, PIPELINE_PCM_5)
PCM_PLAYBACK_ADD(HDMI 1, 5, PIPELINE_PCM_6)
PCM_PLAYBACK_ADD(HDMI 2, 6, PIPELINE_PCM_7)
PCM_PLAYBACK_ADD(HDMI 3, 7, PIPELINE_PCM_8)

#
# BE configurations - overrides config in ACPI if present
#

ifdef(`NOJACK', `',
`
#ALH dai index = ((link_id << 8) | PDI id)
#ALH SDW0 Pin2 (ID: 0)
DAI_CONFIG(ALH, eval(UAJ_LINK * 256 + 2), 0, `SDW'eval(UAJ_LINK)`-Playback',
	ALH_CONFIG(ALH_CONFIG_DATA(ALH, eval(UAJ_LINK * 256 + 2), 48000, 2)))

#ALH SDW0 Pin3 (ID: 1)
DAI_CONFIG(ALH, eval(UAJ_LINK * 256 + 3), 1, `SDW'eval(UAJ_LINK)`-Capture',
	ALH_CONFIG(ALH_CONFIG_DATA(ALH, eval(UAJ_LINK * 256 + 3), 48000, 2)))
')

#ALH SDW1 Pin2 (ID: 2)
DAI_CONFIG(ALH, eval(AMP_1_LINK * 256 + 2), 2, `SDW'eval(AMP_1_LINK)`-Playback',
	ALH_CONFIG(ALH_CONFIG_DATA(ALH, eval(AMP_1_LINK * 256 + 2), 48000, 2)))

ifdef(`NO_AGGREGATION',
`#ALH SDW2 Pin2 (ID: 3)
DAI_CONFIG(ALH, eval(AMP_2_LINK * 256 + 2), 3, `SDW'eval(AMP_2_LINK)`-Playback',
	ALH_CONFIG(ALH_CONFIG_DATA(ALH, eval(AMP_2_LINK * 256 + 2), 48000, 2)))',
`')

#ALH SDW3 Pin2 (ID: 4)
DAI_CONFIG(ALH, eval(MIC_LINK * 256 + 2), 4, `SDW'eval(MIC_LINK)`-Capture',
	ALH_CONFIG(ALH_CONFIG_DATA(ALH, eval(MIC_LINK * 256 + 2), 48000, 2)))

# 3 HDMI/DP outputs (ID: 5,6,7)
DAI_CONFIG(HDA, 0, 5, iDisp1,
	HDA_CONFIG(HDA_CONFIG_DATA(HDA, 0, 48000, 2)))
DAI_CONFIG(HDA, 1, 6, iDisp2,
	HDA_CONFIG(HDA_CONFIG_DATA(HDA, 1, 48000, 2)))
DAI_CONFIG(HDA, 2, 7, iDisp3,
	HDA_CONFIG(HDA_CONFIG_DATA(HDA, 2, 48000, 2)))

DEBUG_END
