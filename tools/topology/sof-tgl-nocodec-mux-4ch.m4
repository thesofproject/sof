#
# Topology for generic Tigerlake board with no codec and digital mic array.
#
# TGL Host GW DMAC support max 6 playback and max 6 capture channels so some
# pipelines/PCMs/DAIs are commented out to keep within HW bounds. If these
# are needed then they can be used provided other PCMs/pipelines/SSPs are
# commented out in their place.

# Include topology builder
include(`utils.m4')
include(`dai.m4')
include(`ssp.m4')
include(`pipeline.m4')

# Include TLV library
include(`common/tlv.m4')

# Include Token library
include(`sof/tokens.m4')

# Include Tigerlake DSP configuration
include(`platform/intel/tgl.m4')
# include(`platform/intel/dmic.m4')

#
# Define the DEMUX bytes
#
include(`bytecontrol.m4')
CONTROLBYTES_PRIV(DEMUX_priv,
`       bytes "0x53,0x4f,0x46,0x00,0x00,0x00,0x00,0x00,'
`       0x28,0x00,0x00,0x00,0x00,0x60,0x00,0x03,'
`       0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'
`       0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'
`       0x02,0x00,0x04,0x00,0x02,0x00,0x00,0x00,'
`       0x02,0x00,0x00,0x00,0x02,0x01,0x02,0x04,'
`       0x08,0x10,0x20,0x40,0x80,0x00,0x00,0x00,'
`       0x03,0x00,0x00,0x00,0x04,0x01,0x02,0x04,'
`       0x08,0x10,0x20,0x40,0x80,0x00,0x00,0x00"'
)
DEBUG_START
#
# Define the pipelines
#
# PCM0 (4ch) ---> volume1-----> SSP0
# PCM0 (2ch) <--- volume2 <-(2ch)-- MUXDEMUX <---- SSP0
#                                     | (4ch)
#                                     V
# PCM3 (4ch) <--- volume3 <-----------+

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate,
dnl     time_domain, sched_comp)

# Low Latency playback pipeline 0 on PCM 2 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	1, 0, 4, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency capture pipeline 1 on PCM 2 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-demux-volume-capture.m4,
	2, 0, 4, s32le,
	1000, 0, 0,
	48000, 48000, 48000)


#
# DAIs configuration
#

dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     deadline, priority, core, time_domain)

# playback DAI is SSP0 using 2 periods
# Buffers use s32le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	1, SSP, 0, NoCodec-0,
	PIPELINE_SOURCE_1, 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# capture DAI is SSP0 using 2 periods
# Buffers use s32le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	2, SSP, 0, NoCodec-0,
	PIPELINE_SINK_2, 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

#
# Echo reference configuration
#

# Low Latency capture pipeline 2 on PCM 3 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-passthrough-capture-sched.m4,
	3, 1, 4, s32le,
	1000, 1, 0,
	48000, 48000, 48000,
	SCHEDULE_TIME_DOMAIN_TIMER,
	PIPELINE_SCHED_COMP_2)

# Connect demux to capture
SectionGraph."PIPE_DEMUX" {
	index "0"

	lines [
		# demux to capture
		dapm(PIPELINE_SINK_3, PIPELINE_DEMUX_2)
	]
}

dnl PCM_DUPLEX_ADD(name, pcm_id, playback, capture)
PCM_DUPLEX_ADD(Port0, 0, PIPELINE_PCM_1, PIPELINE_PCM_2)
PCM_CAPTURE_ADD(EchoRef, 1, PIPELINE_PCM_3)

#
# BE configurations - overrides config in ACPI if present
#
DAI_CONFIG(SSP, 0, 0, NoCodec-0,
	   SSP_CONFIG(DSP_B, SSP_CLOCK(mclk, 38400000, codec_mclk_in),
		      SSP_CLOCK(bclk, 6144000, codec_slave),
		      SSP_CLOCK(fsync, 48000, codec_slave),
		      SSP_TDM(4, 32, 15, 15),
		      SSP_CONFIG_DATA(SSP, 0, 32, 0, SSP_QUIRK_LBM)))

DEBUG_END