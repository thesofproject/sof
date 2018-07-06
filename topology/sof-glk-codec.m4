#
# Topology for Geminilake with headset on SSP2, spk on SSP1 and DMIC capture
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

# Include bxt DSP configuration
include(`platform/intel/bxt.m4')
include(`platform/intel/dmic.m4')

#
# Define the pipelines
#
# PCM0 <---- volume <----- DMIC0 (dmic capture)
# PCM1 ----> volume -----> SSP1 (speaker - maxim98357a)
# PCM2 ----> volume -----> SSP2 (headset - da7219)
#

# Low Latency capture pipeline 1 on PCM 0 using max 4 channels of s32le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_DAI_ADD(sof/pipe-volume-capture.m4,
	1, 0, 4, s32le,
	48, 1000, 0, 0, DMIC, 0, s32le, 2)

# Low Latency playback pipeline 2 on PCM 1 using max 2 channels of s16le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_DAI_ADD(sof/pipe-volume-playback.m4,
	2, 1, 2, s16le,
	48, 1000, 0, 0, SSP, 1, s16le, 2)

# Low Latency playback pipeline 3 on PCM 2 using max 2 channels of s16le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_DAI_ADD(sof/pipe-volume-playback.m4,
	3, 2, 2, s16le,
	48, 1000, 0, 0, SSP, 2, s16le, 2)

#
# DAIs configuration
#

# capture DAI is DMIC0 using 2 periods
# Buffers use s16le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	1, DMIC, 0, DMIC0,
	PIPELINE_SINK_1, 2, s32le,
	48, 1000, 0, 0)

# playback DAI is SSP1 using 2 periods
# Buffers use s16le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	2, SSP, 1, SSP1-Codec,
	PIPELINE_SOURCE_2, 2, s16le,
	48, 1000, 0, 0)

# playback DAI is SSP2 using 2 periods
# Buffers use s16le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	3, SSP, 2, SSP2-Codec,
	PIPELINE_SOURCE_3, 2, s16le,
	48, 1000, 0, 0)

# PCM
PCM_CAPTURE_ADD(DMIC0, 1, 0, 0, PIPELINE_PCM_1)
PCM_PLAYBACK_ADD(SSP1, 2, 1, 1, PIPELINE_PCM_2)
PCM_PLAYBACK_ADD(SSP2, 3, 2, 2, PIPELINE_PCM_3)

#
# BE configurations - overrides config in ACPI if present
#

DAI_CONFIG(DMIC, 0, 0, DMIC0,
	DMIC_CONFIG(1, 500000, 4800000, 40, 60, 48000,
		DMIC_WORD_LENGTH(s32le), DMIC, 0,
		PDM_CONFIG(DMIC, 0, FOUR_CH_PDM0_PDM1)))

#SSP 1 (ID: 1) with 19.2MHz mclk with MCLK_ID 1
DAI_CONFIG(SSP, 1, 1, SSP1-Codec,
	SSP_CONFIG(I2S, SSP_CLOCK(mclk, 19200000, codec_mclk_in),
		SSP_CLOCK(bclk, 1920000, codec_slave),
		SSP_CLOCK(fsync, 48000, codec_slave),
		SSP_TDM(2, 20, 3, 3),
		SSP_CONFIG_DATA(SSP, 1, 16, 1)))

#SSP 2 (ID: 2) with 19.2MHz mclk with MCLK_ID 1
DAI_CONFIG(SSP, 2, 2, SSP2-Codec,
	SSP_CONFIG(I2S, SSP_CLOCK(mclk, 19200000, codec_mclk_in),
		SSP_CLOCK(bclk, 1920000, codec_slave),
		SSP_CLOCK(fsync, 48000, codec_slave),
		SSP_TDM(2, 20, 3, 3),
		SSP_CONFIG_DATA(SSP, 2, 16, 1)))
