#
# Topology for generic Apollolake board with no codec.
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

# Include Apollolake DSP configuration
include(`platform/intel/bxt.m4')
include(`platform/intel/dmic.m4')

#
# Define the pipelines
#
# PCM0 ----> volume -----> SSP0
#      <---- volume <----- SSP0
# PCM1 ----> Volume <----- SSP1
#      <---- Volume <----- SSP1
# PCM2 ----> volume -----> SSP2
#      <---- Volume <----- SSP2
# PCM3 ----> volume -----> SSP3
#      <---- volume <----- SSP3
# PCM4 ----> volume -----> SSP4
#      <---- Volume <----- SSP4
# PCM5 ----> volume -----> SSP5
#      <---- volume <----- SSP5
# PCM6 <---- volume <----- DMIC6 (DMIC01)
#

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     frames, deadline, priority, core)

# Low Latency playback pipeline 1 on PCM 0 using max 2 channels of s16le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	1, 0, 2, s16le,
	48, 1000, 0, 0)

# Low Latency capture pipeline 2 on PCM 0 using max 2 channels of s16le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-capture.m4,
	2, 0, 2, s16le,
	48, 1000, 0, 0)

# Low Latency playback pipeline 3 on PCM 1 using max 2 channels of s16le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	3, 1, 2, s16le,
	48, 1000, 0, 0)

# Low Latency capture pipeline 4 on PCM 1 using max 2 channels of s16le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-capture.m4,
	4, 1, 2, s16le,
	48, 1000, 0, 0)

# Low Latency playback pipeline 5 on PCM 2 using max 2 channels of s16le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	5, 2, 2, s16le,
	48, 1000, 0, 0)

# Low Latency capture pipeline 6 on PCM 2 using max 2 channels of s16le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-capture.m4,
	6, 2, 2, s16le,
	48, 1000, 0, 0)

# Low Latency playback pipeline 7 on PCM 3 using max 2 channels of s16le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	7, 3, 2, s16le,
	48, 1000, 0, 0)

# Low Latency capture pipeline 8 on PCM 3 using max 2 channels of s16le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-capture.m4,
	8, 3, 2, s16le,
	48, 1000, 0, 0)

# Low Latency playback pipeline 9 on PCM 4 using max 2 channels of s16le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	9, 4, 2, s16le,
	48, 1000, 0, 0)

# Low Latency capture pipeline 10 on PCM 4 using max 2 channels of s16le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-capture.m4,
	10, 4, 2, s16le,
	48, 1000, 0, 0)

# Low Latency playback pipeline 11 on PCM 5 using max 2 channels of s16le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	11, 5, 2, s16le,
	48, 1000, 0, 0)

# Low Latency capture pipeline 12 on PCM 5 using max 2 channels of s16le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-capture.m4,
	12, 5, 2, s16le,
	48, 1000, 0, 0)

# Passthrough capture pipeline 13 on PCM 6 using max 4 channels.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-passthrough-capture.m4,
	13, 6, 4, s32le,
	48, 1000, 0, 0)

#
# DAIs configuration
#

dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     frames, deadline, priority, core)

# playback DAI is SSP0 using 2 periods
# Buffers use s16le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	1, SSP, 0, NoCodec-0,
	PIPELINE_SOURCE_1, 2, s16le,
	48, 1000, 0, 0)

# capture DAI is SSP0 using 2 periods
# Buffers use s16le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	2, SSP, 0, NoCodec-0,
	PIPELINE_SINK_2, 2, s16le,
	48, 1000, 0, 0)

# playback DAI is SSP1 using 2 periods
# Buffers use s16le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	3, SSP, 1, NoCodec-1,
	PIPELINE_SOURCE_3, 2, s16le,
	48, 1000, 0, 0)

# capture DAI is SSP1 using 2 periods
# Buffers use s16le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	4, SSP, 1, NoCodec-1,
	PIPELINE_SINK_4, 2, s16le,
	48, 1000, 0, 0)

# playback DAI is SSP2 using 2 periods
# Buffers use s16le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	5, SSP, 2, NoCodec-2,
	PIPELINE_SOURCE_5, 2, s16le,
	48, 1000, 0, 0)

# capture DAI is SSP2 using 2 periods
# Buffers use s16le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	6, SSP, 2, NoCodec-2,
	PIPELINE_SINK_6, 2, s16le,
	48, 1000, 0, 0)

# playback DAI is SSP3 using 2 periods
# Buffers use s16le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	7, SSP, 3, NoCodec-3,
	PIPELINE_SOURCE_7, 2, s16le,
	48, 1000, 0, 0)

# capture DAI is SSP3 using 2 periods
# Buffers use s16le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	8, SSP, 3, NoCodec-3,
	PIPELINE_SINK_8, 2, s16le,
	48, 1000, 0, 0)

# playback DAI is SSP4 using 2 periods
# Buffers use s16le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	9, SSP, 4, NoCodec-4,
	PIPELINE_SOURCE_9, 2, s16le,
	48, 1000, 0, 0)

# capture DAI is SSP4 using 2 periods
# Buffers use s16le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	10, SSP, 4, NoCodec-4,
	PIPELINE_SINK_10, 2, s16le,
	48, 1000, 0, 0)

# playback DAI is SSP5 using 2 periods
# Buffers use s16le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	11, SSP, 5, NoCodec-5,
	PIPELINE_SOURCE_11, 2, s16le,
	48, 1000, 0, 0)

# capture DAI is SSP5 using 2 periods
# Buffers use s16le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	12, SSP, 5, NoCodec-5,
	PIPELINE_SINK_12, 2, s16le,
	48, 1000, 0, 0)

# capture DAI is DMIC 0 using 2 periods
# Buffers use s32le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	13, DMIC, 0, NoCodec-6,
	PIPELINE_SINK_13, 2, s32le,
	48, 1000, 0, 0)

dnl PCM_DUPLEX_ADD(name, pcm_id, playback, capture)
PCM_DUPLEX_ADD(Port0, 0, PIPELINE_PCM_1, PIPELINE_PCM_2)
PCM_DUPLEX_ADD(Port1, 1, PIPELINE_PCM_3, PIPELINE_PCM_4)
PCM_DUPLEX_ADD(Port2, 2, PIPELINE_PCM_5, PIPELINE_PCM_6)
PCM_DUPLEX_ADD(Port3, 3, PIPELINE_PCM_7, PIPELINE_PCM_8)
PCM_DUPLEX_ADD(Port4, 4, PIPELINE_PCM_9, PIPELINE_PCM_10)
PCM_DUPLEX_ADD(Port5, 5, PIPELINE_PCM_11, PIPELINE_PCM_12)
dnl PCM_CAPTURE_ADD(name, pipeline, capture)
PCM_CAPTURE_ADD(DMIC01, 6, PIPELINE_PCM_13)

#
# BE configurations - overrides config in ACPI if present
#

dnl DAI_CONFIG(type, dai_index, link_id, name, ssp_config/dmic_config)
DAI_CONFIG(SSP, 0, 0, NoCodec-0,
	   dnl SSP_CONFIG(format, mclk, bclk, fsync, tdm, ssp_config_data)
	   SSP_CONFIG(I2S, SSP_CLOCK(mclk, 24576000, codec_mclk_in),
		      SSP_CLOCK(bclk, 1536000, codec_slave),
		      SSP_CLOCK(fsync, 48000, codec_slave),
		      SSP_TDM(2, 16, 3, 3),
		      dnl SSP_CONFIG_DATA(type, dai_index, valid bits, mclk_id)
		      SSP_CONFIG_DATA(SSP, 0, 16)))

DAI_CONFIG(SSP, 1, 1, NoCodec-1,
	   SSP_CONFIG(I2S, SSP_CLOCK(mclk, 24576000, codec_mclk_in),
		      SSP_CLOCK(bclk, 1536000, codec_slave),
		      SSP_CLOCK(fsync, 48000, codec_slave),
		      SSP_TDM(2, 16, 3, 3),
		      SSP_CONFIG_DATA(SSP, 1, 16)))

DAI_CONFIG(SSP, 2, 2, NoCodec-2,
	   SSP_CONFIG(I2S, SSP_CLOCK(mclk, 24576000, codec_mclk_in),
		      SSP_CLOCK(bclk, 1536000, codec_slave),
		      SSP_CLOCK(fsync, 48000, codec_slave),
		      SSP_TDM(2, 16, 3, 3),
		      SSP_CONFIG_DATA(SSP, 2, 16)))

DAI_CONFIG(SSP, 3, 3, NoCodec-3,
	   SSP_CONFIG(I2S, SSP_CLOCK(mclk, 24576000, codec_mclk_in),
		      SSP_CLOCK(bclk, 1536000, codec_slave),
		      SSP_CLOCK(fsync, 48000, codec_slave),
		      SSP_TDM(2, 16, 3, 3),
		      SSP_CONFIG_DATA(SSP, 3, 16)))

DAI_CONFIG(SSP, 4, 4, NoCodec-4,
	   SSP_CONFIG(I2S, SSP_CLOCK(mclk, 24576000, codec_mclk_in),
		      SSP_CLOCK(bclk, 1536000, codec_slave),
		      SSP_CLOCK(fsync, 48000, codec_slave),
		      SSP_TDM(2, 16, 3, 3),
		      SSP_CONFIG_DATA(SSP, 4, 16)))

DAI_CONFIG(SSP, 5, 5, NoCodec-5,
	   SSP_CONFIG(I2S, SSP_CLOCK(mclk, 24576000, codec_mclk_in),
		      SSP_CLOCK(bclk, 1536000, codec_slave),
		      SSP_CLOCK(fsync, 48000, codec_slave),
		      SSP_TDM(2, 16, 3, 3),
		      SSP_CONFIG_DATA(SSP, 5, 16)))

DAI_CONFIG(DMIC, 0, 6, NoCodec-6,
	   dnl DMIC_CONFIG(driver_version, clk_min, clk_mac, duty_min, duty_max,
	   dnl		   sample_rate,
	   dnl		   fifo word length, type, dai_index, pdm controller config)
	   DMIC_CONFIG(1, 500000, 4800000, 40, 60, 48000,
		dnl DMIC_WORD_LENGTH(frame_format)
		DMIC_WORD_LENGTH(s32le), DMIC, 0,
		dnl PDM_CONFIG(type, dai_index, num pdm active, pdm tuples list)
		dnl STEREO_PDM0 is a pre-defined pdm config for stereo capture
		PDM_CONFIG(DMIC, 0, STEREO_PDM0)))

