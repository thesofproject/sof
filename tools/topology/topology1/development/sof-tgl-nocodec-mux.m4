#
# Topology for Tigerlake RVP board CI testing, with covering as more
# features as possible running with no codec machine.
#
# TGL Host GW DMAC support max 6 playback and max 6 capture channels so some
# pipelines/PCMs/DAIs are commented out to keep within HW bounds. If these
# are needed then they can be used provided other PCMs/pipelines/SSPs are
# commented out in their place.

# Include topology builder
include(`utils.m4')
include(`dai.m4')
include(`ssp.m4')
include(`muxdemux.m4')
include(`bytecontrol.m4')
include(`pipeline.m4')

# Include TLV library
include(`common/tlv.m4')

# Include Token library
include(`sof/tokens.m4')

# Include Tigerlake DSP configuration
include(`platform/intel/tgl.m4')

define(PIPE_NAME, pipe-tgl-mux)

#
# Define the demux configure
#
dnl Configure demux
dnl name, pipeline_id, routing_matrix_rows
dnl Diagonal 1's in routing matrix mean that every input channel is
dnl copied to corresponding output channels in all output streams.
dnl I.e. row index is the input channel, 1 means it is copied to
dnl corresponding output channel (column index), 0 means it is discarded.
dnl There's a separate matrix for all outputs.
define(matrix1, `ROUTE_MATRIX(2,
			     `BITS_TO_BYTE(1, 0, 0 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 1, 0 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,0 ,0 ,0)')')

define(matrix2, `ROUTE_MATRIX(3,
			     `BITS_TO_BYTE(0, 0, 1 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,1 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,0 ,0 ,0)')')

dnl name, num_streams, route_matrix list
MUXDEMUX_CONFIG(demux_priv_1, 2, LIST_NONEWLINE(`', `matrix1,', `matrix2'))

#define SSP_MCLK
define(`SSP_MCLK', 38400000)

# playback DAI is SSP0 using 2 periods
# Buffers use s16le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-mux-dai-playback.m4,
	1, SSP, 0, NoCodec-0,
	NOT_USED_IGNORED, 2, s16le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER,
	4, 48000)

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate,
dnl     time_domain, sched_comp)

# Low Latency playback pipeline 1 on PCM 0 using max 2 channels of s16le.
# Schedule 48 frames per 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-host-playback.m4,
	2, 0, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000,
	SCHEDULE_TIME_DOMAIN_TIMER,
	PIPELINE_PLAYBACK_SCHED_COMP_1)

# Low Latency playback pipeline 3 on PCM 1 using max 2 channels of s16le.
# Schedule 48 frames per 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-host-playback.m4,
	3, 1, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000,
	SCHEDULE_TIME_DOMAIN_TIMER,
	PIPELINE_PLAYBACK_SCHED_COMP_1)

# Volume-switch capture pipeline 2 on PCM 0 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-passthrough-capture.m4,
	4, 0, 4, s32le,
	1000, 0, 0,
	48000, 48000, 48000)
#
# DAIs configuration
#

dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     deadline, priority, core, time_domain)

# capture DAI is SSP0 using 2 periods
# Buffers use s16le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	4, SSP, 0, NoCodec-0,
	PIPELINE_SINK_4, 2, s16le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# Connect pipelines together
SectionGraph."PIPE_NAME" {
	index "0"
	lines [
		# PCM pipeline 3 to DAI pipeline 1
		dapm(PIPELINE_MUX_1, PIPELINE_SOURCE_2)
		# PCM pipeline 4 to DAI pipeline 1
		dapm(PIPELINE_MUX_1, PIPELINE_SOURCE_3)
	]
}

dnl PCM_DUPLEX_ADD(name, pcm_id, playback, capture)
PCM_DUPLEX_ADD(Port0, 0, PIPELINE_PCM_2, PIPELINE_PCM_4)
PCM_PLAYBACK_ADD(Media Playback MUX 1, 1, PIPELINE_PCM_3)

#
# BE configurations - overrides config in ACPI if present
#

DAI_CONFIG(SSP, 0, 0, NoCodec-0,
		SSP_CONFIG(I2S, SSP_CLOCK(mclk, 38400000, codec_mclk_in),
			SSP_CLOCK(bclk, 4800000, codec_slave),
			SSP_CLOCK(fsync, 48000, codec_slave),
			SSP_TDM(4, 25, 15, 15),
			SSP_CONFIG_DATA(SSP, 0, 24, 0, SSP_QUIRK_LBM)))
