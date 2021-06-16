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
include(`platform/intel/dmic.m4')

#
# Define the pipelines
#
# PCM0 <---> volume <----> SSP0
# PCM1 <---> Volume <----> SSP1
# PCM2 <---> volume <----> SSP2
# PCM6 <---- volume <----- DMIC6 (DMIC01)
#

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate,
dnl     time_domain, sched_comp)

# Low Latency playback pipeline 1 on PCM 0 using max 2 channels of s16le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	1, 0, 2, s16le,
	1000, 0, 0,
	48000, 48000, 48000)

# Volume switch capture pipeline 2 on PCM 0 using max 2 channels of s16le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-switch-capture.m4,
	2, 0, 2, s16le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 3 on PCM 1 using max 2 channels of s16le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	3, 1, 2, s16le,
	1000, 0, 0,
	48000, 48000, 48000)

# Volume switch capture pipeline 4 on PCM 1 using max 2 channels of s16le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-switch-capture.m4,
	4, 1, 2, s16le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 5 on PCM 2 using max 2 channels of s16le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	5, 2, 2, s16le,
	1000, 0, 0,
	48000, 48000, 48000)

# Volume switch capture pipeline 6 on PCM 2 using max 2 channels of s16le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-switch-capture.m4,
	6, 2, 2, s16le,
	1000, 0, 0,
	48000, 48000, 48000)

# Passthrough capture pipeline 13 on PCM 6 using max 2 channels.
# 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-passthrough-capture.m4,
	13, 6, 2, s32le,
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
# Buffers use s16le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	1, SSP, 0, NoCodec-0,
	PIPELINE_SOURCE_1, 2, s16le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# capture DAI is SSP0 using 2 periods
# Buffers use s16le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	2, SSP, 0, NoCodec-0,
	PIPELINE_SINK_2, 2, s16le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is SSP1 using 2 periods
# Buffers use s16le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	3, SSP, 1, NoCodec-1,
	PIPELINE_SOURCE_3, 2, s16le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# capture DAI is SSP1 using 2 periods
# Buffers use s16le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	4, SSP, 1, NoCodec-1,
	PIPELINE_SINK_4, 2, s16le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is SSP2 using 2 periods
# Buffers use s16le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	5, SSP, 2, NoCodec-2,
	PIPELINE_SOURCE_5, 2, s16le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# capture DAI is SSP2 using 2 periods
# Buffers use s16le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	6, SSP, 2, NoCodec-2,
	PIPELINE_SINK_6, 2, s16le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# capture DAI is DMIC 0 using 2 periods
# Buffers use s32le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	13, DMIC, 0, NoCodec-6,
	PIPELINE_SINK_13, 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

dnl PCM_DUPLEX_ADD(name, pcm_id, playback, capture)
PCM_DUPLEX_ADD(Port0, 0, PIPELINE_PCM_1, PIPELINE_PCM_2)
PCM_DUPLEX_ADD(Port1, 1, PIPELINE_PCM_3, PIPELINE_PCM_4)
PCM_DUPLEX_ADD(Port2, 2, PIPELINE_PCM_5, PIPELINE_PCM_6)
PCM_CAPTURE_ADD(DMIC, 6, PIPELINE_PCM_13)

#
# BE configurations - overrides config in ACPI if present
#

DAI_CONFIG(SSP, 0, 0, NoCodec-0,
	   SSP_CONFIG(I2S, SSP_CLOCK(mclk, 38400000, codec_mclk_in),
		      SSP_CLOCK(bclk, 2400000, codec_slave),
		      SSP_CLOCK(fsync, 48000, codec_slave),
		      SSP_TDM(2, 25, 3, 3),
		      SSP_CONFIG_DATA(SSP, 0, 24, 0, SSP_QUIRK_LBM)))

DAI_CONFIG(SSP, 1, 1, NoCodec-1,
	   SSP_CONFIG(I2S, SSP_CLOCK(mclk, 38400000, codec_mclk_in),
		      SSP_CLOCK(bclk, 2400000, codec_slave),
		      SSP_CLOCK(fsync, 48000, codec_slave),
		      SSP_TDM(2, 25, 3, 3),
		      SSP_CONFIG_DATA(SSP, 1, 24, 0, SSP_QUIRK_LBM)))

DAI_CONFIG(SSP, 2, 2, NoCodec-2,
	   SSP_CONFIG(I2S, SSP_CLOCK(mclk, 38400000, codec_mclk_in),
		      SSP_CLOCK(bclk, 2400000, codec_slave),
		      SSP_CLOCK(fsync, 48000, codec_slave),
		      SSP_TDM(2, 25, 3, 3),
		      SSP_CONFIG_DATA(SSP, 2, 24, 0, SSP_QUIRK_LBM)))

DAI_CONFIG(DMIC, 0, 6, NoCodec-6,
	   dnl DMIC_CONFIG(driver_version, clk_min, clk_mac, duty_min, duty_max,
	   dnl		   sample_rate, fifo word length, unmute time, type,
	   dnl		   dai_index, pdm controller config)
	   DMIC_CONFIG(1, 2400000, 4800000, 40, 60, 48000,
		DMIC_WORD_LENGTH(s32le), 400, DMIC, 0,
		dnl PDM_CONFIG(type, dai_index, num pdm active, pdm tuples list)
		dnl STEREO_PDM0 is a pre-defined pdm config for stereo capture
		PDM_CONFIG(DMIC, 0, STEREO_PDM0)))

