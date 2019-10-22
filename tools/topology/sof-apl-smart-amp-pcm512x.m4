#
# Topology for generic Apollolake UP^2 with pcm512x codec with equalizer components.
#

# Include topology builder
include(`utils.m4')
include(`dai.m4')
include(`pipeline.m4')
include(`ssp.m4')

# Include TLV library
include(`common/tlv.m4')

# Include Token library
include(`sof/tokens.m4')

# Include Apollolake DSP configuration
include(`platform/intel/bxt.m4')

#
# Define the pipelines
#
# PCM0 --> buf --> volume --> Smart AMP --> buf --> SSP5 (pcm512x)
#                                ^
#                                |
#                               buf
#                                ^
#                                |
# PCM0 --> buf --> volume --> Smart AMP --> buf --> SSP5 (pcm512x)
#

dnl PIPELINE_PCM_DAI_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     dai type, dai_index, dai format,
dnl     dai periods, pcm_min_rate, pcm_max_rate,
dnl     pipeline_rate, time_domain)

# Smart AMP playback pipeline 1 on PCM 0 using max 2 channels of s16le.
# Set 1000us deadline on core 0 with priority 0
PIPELINE_PCM_DAI_ADD(sof/pipe-volume-smart-amp-playback.m4,
	1, 0, 2, s16le,
	1000, 0, 0, SSP, 5, s16le, 3,
	48000, 48000, 48000)

# Smart AMP capture pipeline 2 on PCM 0 using max 2 channels of s16le.
# Set 1000us deadline on core 0 with priority 0
PIPELINE_PCM_DAI_ADD(sof/pipe-volume-smart-amp-capture.m4,
	2, 0, 2, s16le,
	1000, 0, 0, SSP, 5, s16le, 3,
	48000, 48000, 48000)

#
# DAIs configuration
#

dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     deadline, priority, core, time_domain)

# playback DAI is SSP5 using 3 periods
# Buffers use s16le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	1, SSP, 5, SSP5-Codec,
	PIPELINE_SOURCE_1, 3, s16le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# capture DAI is SSP5 using 3 periods
# Buffers use s16le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	2, SSP, 5, SSP5-Codec,
	PIPELINE_SINK_2, 3, s16le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)


# PCM Low Latency, id 0
PCM_DUPLEX_ADD(Port5, 0, PIPELINE_PCM_1, PIPELINE_PCM_2)

# Connect pipelines together
SectionGraph."smart-amp-pipeline" {
	index "0"

	lines [
		dapm(PIPELINE_SMART_AMP_1, PIPELINE_SMART_AMP_2)
	]
}


#
# BE configurations - overrides config in ACPI if present
#

DAI_CONFIG(SSP, 5, 0, SSP5-Codec,
	SSP_CONFIG(I2S, SSP_CLOCK(mclk, 24576000, codec_mclk_in),
		SSP_CLOCK(bclk, 1536000, codec_slave),
		SSP_CLOCK(fsync, 48000, codec_slave),
		SSP_TDM(2, 16, 3, 3),
		SSP_CONFIG_DATA(SSP, 5, 16, 0, SSP_QUIRK_LBM)))
