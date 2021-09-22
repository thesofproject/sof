#
# Topology for generic Apollolake board with no codec and digital mic array.
#
# APL Host GW DMAC support max 6 playback and max 6 capture channels so some
# pipelines/PCMs/DAIs are commented out to keep within HW bounds. If these
# are needed then they can be used provided other PCMs/pipelines/SSPs are
# commented out in their place.

# Include topology builder
include(`utils.m4')
include(`dai.m4')
include(`ssp.m4')
include(`pipeline.m4')
include(`muxdemux.m4')

# Include TLV library
include(`common/tlv.m4')

# Include Token library
include(`sof/tokens.m4')

# Include Apollolake DSP configuration
include(`platform/intel/bxt.m4')
include(`platform/intel/dmic.m4')

dnl Configure demux
dnl name, pipeline_id, routing_matrix_rows
dnl Diagonal 1's in routing matrix mean that every input channel is
dnl copied to corresponding output channels in all output streams.
dnl I.e. row index is the input channel, 1 means it is copied to
dnl corresponding output channel (column index), 0 means it is discarded.
dnl There's a separate matrix for all outputs.
define(matrix1, `ROUTE_MATRIX(1,
			     `BITS_TO_BYTE(1, 0, 0 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 1, 0 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(1, 0, 0 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 1, 0 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,0 ,0 ,0)')')

dnl name, num_streams, route_matrix list
MUXDEMUX_CONFIG(demux_priv_1, 1, LIST(`	', `matrix1'))

#
# Define the pipelines
#
# PCM0P ---> demux ---> eq_iir ---> SSP0
# PCM0C <-------------------------- SSP0

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate,
dnl     time_domain, sched_comp)

# Low Latency playback pipeline 1 on PCM 0 using max 2 channels of s32le.
# Set 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-demux-eq-iir-playback.m4,
	1, 0, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Volume switch capture pipeline 2 on PCM 0 using max 4 channels of s32le.
# 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-passthrough-capture.m4,
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
# Buffers use s32le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	1, SSP, 0, NoCodec-0,
	PIPELINE_SOURCE_1, 4, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)


# capture DAI is SSP0 using 2 periods
# Buffers use s32le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	2, SSP, 0, NoCodec-0,
	PIPELINE_SINK_2, 4, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)


dnl PCM_DUPLEX_ADD(name, pcm_id, playback, capture)
PCM_DUPLEX_ADD(Port0, 0, PIPELINE_PCM_1, PIPELINE_PCM_2)

#
# BE configurations - overrides config in ACPI if present
#

dnl DAI_CONFIG(type, dai_index, link_id, name, ssp_config/dmic_config)
DAI_CONFIG(SSP, 0, 0, NoCodec-0,
	   dnl SSP_CONFIG(format, mclk, bclk, fsync, tdm, ssp_config_data)
	   SSP_CONFIG(I2S, SSP_CLOCK(mclk, 24576000, codec_mclk_in),
		      SSP_CLOCK(bclk, 6144000, codec_slave),
		      SSP_CLOCK(fsync, 48000, codec_slave),
		      SSP_TDM(4, 32, 15, 15),
		      dnl SSP_CONFIG_DATA(type, dai_index, valid bits, mclk_id, quirks)
		      SSP_CONFIG_DATA(SSP, 0, 32, 0, SSP_QUIRK_LBM)))
