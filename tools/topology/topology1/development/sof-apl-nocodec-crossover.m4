#
# Topology for generic Apollolake board in nocodec mode
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
include(`crossover.m4')
include(`buffer.m4')

# Include TLV library
include(`common/tlv.m4')

# Include Token library
include(`sof/tokens.m4')

# Include Apollolake DSP configuration
include(`platform/intel/bxt.m4')
include(`platform/intel/dmic.m4')

#
# Define the pipelines
#
# PCM0P --buf1.0--> crossover1.0 --buf1.1--> SSP0
#                     \------------buf2.0--> SSP1
# PCM0C <-----------buf3.0------------------ SSP0
# PCM1C <-----------buf4.0------------------ SSP1

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate,
dnl     time_domain, sched_comp)

# Low Latency playback pipeline 1 on PCM 0 using max 2 channels of s32le.
# Set 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-crossover-playback.m4,
	1, 0, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Passthrough capture pipeline 3 on PCM 0 using max 2 channels of s32le.
# 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-passthrough-capture.m4,
	3, 0, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Passthrough capture pipeline 4 on PCM 1 using max 2 channels of s32le.
# 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-passthrough-capture.m4,
	4, 1, 2, s32le,
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
# Buffers use s32le format, 1000us deadline with priority 0 on core 0
DAI_ADD(sof/pipe-dai-playback.m4,
	1, SSP, 0, NoCodec-0,
	PIPELINE_SOURCE_1, 4, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER, 2, 48000)

# playback DAI is SSP1 using 2 periods
# Buffers use s32le format, 1000us deadline with priority 0 on core 0
DAI_ADD(sof/pipe-passthrough-dai-playback.m4,
	2, SSP, 1, NoCodec-1,
	PIPELINE_SOURCE_2, 4, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER, 2, 48000)

# capture DAI is SSP0 using 2 periods
# Buffers use s32le format, 1000us deadline with priority 0 on core 0
DAI_ADD(sof/pipe-dai-capture.m4,
	3, SSP, 0, NoCodec-0,
	PIPELINE_SINK_3, 4, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER, 2, 48000)

# capture DAI is SSP1 using 2 periods
# Buffers use s32le format, 1000us deadline with priority 0 on core 0
DAI_ADD(sof/pipe-dai-capture.m4,
	4, SSP, 1, NoCodec-1,
	PIPELINE_SINK_4, 4, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER, 2, 48000)


SectionGraph."crossover-buffer" {
	index "2"

	lines [
		dapm(PIPELINE_SOURCE_2, PIPELINE_CROSSOVER_1)
	]
}

dnl PCM_DUPLEX_ADD(name, pcm_id, playback, capture)
PCM_DUPLEX_ADD(Port0, 0, PIPELINE_PCM_1, PIPELINE_PCM_3)

dnl PCM_CAPTURE_ADD(name, pipeline, capture)
PCM_CAPTURE_ADD(Port1, 1, PIPELINE_PCM_4)

#
# BE configurations - overrides config in ACPI if present
#

define(SSP0_IDX, `0')
define(SSP1_IDX, `1')

DAI_CONFIG(SSP, SSP0_IDX, 0, NoCodec-0,
	   SSP_CONFIG(I2S, SSP_CLOCK(mclk, 24576000, codec_mclk_in),
		      SSP_CLOCK(bclk, 3072000, codec_slave),
		      SSP_CLOCK(fsync, 48000, codec_slave),
		      SSP_TDM(2, 32, 3, 3),
		      dnl SSP_CONFIG_DATA(type, idx, valid bits, mclk_id, quirks, bclk_delay,
		      dnl clks_control, pulse_width, padding)
		      SSP_CONFIG_DATA(SSP, SSP0_IDX, 32, 0, SSP_QUIRK_LBM, 0,
				      eval(SSP_CC_MCLK_ES | SSP_CC_BCLK_ES))))

DAI_CONFIG(SSP, SSP1_IDX, 1, NoCodec-1,
	   SSP_CONFIG(I2S, SSP_CLOCK(mclk, 24576000, codec_mclk_in),
		      SSP_CLOCK(bclk, 3072000, codec_slave),
		      SSP_CLOCK(fsync, 48000, codec_slave),
		      SSP_TDM(2, 32, 3, 3),
		      SSP_CONFIG_DATA(SSP, SSP1_IDX, 32, 0, SSP_QUIRK_LBM, 0,
				      eval(SSP_CC_MCLK_ES | SSP_CC_BCLK_ES))))
