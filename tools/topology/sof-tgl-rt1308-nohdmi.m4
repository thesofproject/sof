#
# Topology for Tigerlake with rt1308 codec + DMIC + 4 HDMI
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

# Include Tigerlake DSP configuration
include(`platform/intel/tgl.m4')
include(`platform/intel/dmic.m4')

DEBUG_START

#
# Define the pipelines
#
# PCM0 ----> volume -----> SSP2
# PCM1 <---- volume <----- DMIC01 (dmic0 capture)
#

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     frames, deadline, priority, core)

# Low Latency playback pipeline 1 on PCM 0 using max 2 channels of s24le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	1, 0, 2, s24le,
	1000, 0, 0,
	48000, 48000, 48000)

# Passthrough capture pipeline 3 on PCM 1 using max 4 channels.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-passthrough-capture.m4,
	2, 1, 4, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

#
# DAIs configuration
#

dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     frames, deadline, priority, core)

# playback DAI is SSP2 using 2 periods
# Buffers use s24le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	1, SSP, 2, SSP2-Codec,
	PIPELINE_SOURCE_1, 2, s24le,
	1000, 0, 0)

# capture DAI is DMIC01 using 2 periods
# Buffers use s32le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	2, DMIC, 0, dmic01,
	PIPELINE_SINK_2, 2, s32le,
	1000, 0, 0)

# PCM Low Latency, id 0
dnl PCM_PLAYBACK_ADD(name, pcm_id, playback)
PCM_PLAYBACK_ADD(Speaker, 0, PIPELINE_PCM_1)
PCM_CAPTURE_ADD(DMIC01, 1, PIPELINE_PCM_2)

#
# BE configurations - overrides config in ACPI if present
#
#SSP 2 (ID: 0)
DAI_CONFIG(SSP, 2, 0, SSP2-Codec,
	SSP_CONFIG(I2S, SSP_CLOCK(mclk, 38400000, codec_mclk_in),
		      SSP_CLOCK(bclk, 2400000, codec_slave),
		      SSP_CLOCK(fsync, 48000, codec_slave),
		      SSP_TDM(2, 25, 3, 3),
		      SSP_CONFIG_DATA(SSP, 2, 24)))

# dmic01 (ID: 1)
DAI_CONFIG(DMIC, 0, 1, dmic01,
	   DMIC_CONFIG(1, 500000, 4800000, 40, 60, 48000,
		DMIC_WORD_LENGTH(s32le), 400, DMIC, 0,
		PDM_CONFIG(DMIC, 0, FOUR_CH_PDM0_PDM1)))

DEBUG_END
