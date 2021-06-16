#
# Topology for generic Cannonlake board with no codec and digital mic array.
#

# Include topology builder
include(`utils.m4')
include(`dai.m4')
include(`ssp.m4')
include(`pipeline.m4')

# Include TLV library
include(`common/tlv.m4')

# Include Token library
include(`sof/tokens.m4')

# Include Cannonlake DSP configuration
include(`platform/intel/cnl.m4')
include(`platform/intel/dmic.m4')

#
# Define the pipelines
#
# PCM0 <---> volume <----> SSP0
# PCM1 <---> Volume <----> SSP1
# PCM2 <---> volume <----> SSP2
# PCM3 <---- volume <----- DMIC3 (DMIC01)
#

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate,
dnl     time_domain, sched_comp)

# Low Latency playback pipeline 1 on PCM 0 using max 2 channels of s24le.
# Set 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	1, 0, 2, s24le,
	1000, 0, 0,
	48000, 48000, 48000)

# Volume switch capture pipeline 2 on PCM 0 using max 2 channels of s24le.
# Set 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-switch-capture.m4,
	2, 0, 2, s24le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 3 on PCM 1 using max 2 channels of s24le.
# Set 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	3, 1, 2, s24le,
	1000, 0, 0,
	48000, 48000, 48000)

# Volume switch capture pipeline 4 on PCM 1 using max 2 channels of s24le.
# Set 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-switch-capture.m4,
	4, 1, 2, s24le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 5 on PCM 2 using max 2 channels of s24le.
# Set 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	5, 2, 2, s24le,
	1000, 0, 0,
	48000, 48000, 48000)

# Volume switch capture pipeline 6 on PCM 2 using max 2 channels of s24le.
# Set 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-switch-capture.m4,
	6, 2, 2, s24le,
	1000, 0, 0,
	48000, 48000, 48000)

# Passthrough capture pipeline 7 on PCM 3 using max 4 channels.
# Set 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-passthrough-capture.m4,
	7, 3, 4, s32le,
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
# Buffers use s24le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	1, SSP, 0, NoCodec-0,
	PIPELINE_SOURCE_1, 2, s24le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# capture DAI is SSP0 using 2 periods
# Buffers use s24le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	2, SSP, 0, NoCodec-0,
	PIPELINE_SINK_2, 2, s24le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is SSP1 using 2 periods
# Buffers use s24le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	3, SSP, 1, NoCodec-1,
	PIPELINE_SOURCE_3, 2, s24le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# capture DAI is SSP1 using 2 periods
# Buffers use s24le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	4, SSP, 1, NoCodec-1,
	PIPELINE_SINK_4, 2, s24le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is SSP2 using 2 periods
# Buffers use s24le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	5, SSP, 2, NoCodec-2,
	PIPELINE_SOURCE_5, 2, s24le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# capture DAI is SSP2 using 2 periods
# Buffers use s24le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	6, SSP, 2, NoCodec-2,
	PIPELINE_SINK_6, 2, s24le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# capture DAI is DMIC 0 using 2 periods
# Buffers use s32le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	7, DMIC, 0, NoCodec-3,
	PIPELINE_SINK_7, 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

dnl PCM_DUPLEX_ADD(name, pcm_id, playback, capture)
PCM_DUPLEX_ADD(Port0, 0, PIPELINE_PCM_1, PIPELINE_PCM_2)
PCM_DUPLEX_ADD(Port1, 1, PIPELINE_PCM_3, PIPELINE_PCM_4)
PCM_DUPLEX_ADD(Port2, 2, PIPELINE_PCM_5, PIPELINE_PCM_6)
dnl PCM_CAPTURE_ADD(name, pipeline, capture)
PCM_CAPTURE_ADD(DMIC, 3, PIPELINE_PCM_7)

#
# BE configurations - overrides config in ACPI if present
#

dnl DAI_CONFIG(type, dai_index, link_id, name, ssp_config/dmic_config)
DAI_CONFIG(SSP, 0, 0, NoCodec-0,
	   dnl SSP_CONFIG(format, mclk, bclk, fsync, tdm, ssp_config_data)
	   SSP_CONFIG(I2S, SSP_CLOCK(mclk, 24000000, codec_mclk_in),
		      SSP_CLOCK(bclk, 4800000, codec_slave),
		      SSP_CLOCK(fsync, 48000, codec_slave),
		      SSP_TDM(2, 25, 3, 3),
		      dnl SSP_CONFIG_DATA(type, dai_index, valid bits, mclk_id, quirks)
		      SSP_CONFIG_DATA(SSP, 0, 24, 0, SSP_QUIRK_LBM)))

DAI_CONFIG(SSP, 1, 1, NoCodec-1,
	   SSP_CONFIG(I2S, SSP_CLOCK(mclk, 24000000, codec_mclk_in),
		      SSP_CLOCK(bclk, 4800000, codec_slave),
		      SSP_CLOCK(fsync, 48000, codec_slave),
		      SSP_TDM(2, 25, 3, 3),
		      SSP_CONFIG_DATA(SSP, 1, 24, 0, SSP_QUIRK_LBM)))

DAI_CONFIG(SSP, 2, 2, NoCodec-2,
	   SSP_CONFIG(I2S, SSP_CLOCK(mclk, 24000000, codec_mclk_in),
		      SSP_CLOCK(bclk, 4800000, codec_slave),
		      SSP_CLOCK(fsync, 48000, codec_slave),
		      SSP_TDM(2, 25, 3, 3),
		      SSP_CONFIG_DATA(SSP, 2, 24, 0, SSP_QUIRK_LBM)))

DAI_CONFIG(DMIC, 0, 3, NoCodec-3,
	   DMIC_CONFIG(1, 2400000, 4800000, 40, 60, 48000,
		DMIC_WORD_LENGTH(s32le), 400, DMIC, 0,
		PDM_CONFIG(DMIC, 0, FOUR_CH_PDM0_PDM1)))
