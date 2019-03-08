#
# Topology for GeminiLake with MAX98357.
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

#
# Define the pipelines
#
# PCM0  ----> SRC (pipe 1)   -----> SSP1 (speaker - maxim98357a, BE link 0)
#

# Low Latency playback pipeline 1 on PCM 0 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-src-playback.m4,
	1, 0, 2, s32le,
	48, 1000, 0, 0)

#
# DAIs configuration
#

# playback DAI is SSP1 using 2 periods
# Buffers use s32le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	1, SSP, 1, SSP1-Codec,
	PIPELINE_SOURCE_1, 2, s32le,
	48, 1000, 0, 0)

## playback DAI is SSP1 using 2 periods
## Buffers use s24le format, with 48 frame per 1000us on core 0 with priority 0
#DAI_ADD(sof/pipe-dai-playback.m4,
#	1, SSP, 1, SSP1-Codec,
#	PIPELINE_SOURCE_1, 2, s24le,
#	48, 1000, 0, 0)

## playback DAI is SSP1 using 2 periods
## Buffers use s16le format, with 48 frame per 1000us on core 0 with priority 0
#DAI_ADD(sof/pipe-dai-playback.m4,
#	1, SSP, 1, SSP1-Codec,
#	PIPELINE_SOURCE_1, 2, s16le,
#	48, 1000, 0, 0)

PCM_PLAYBACK_ADD(Speakers, 0, PIPELINE_PCM_1)

#
# BE configurations - overrides config in ACPI if present
#

#SSP 1 (ID: 0) with 19.2 MHz mclk with MCLK_ID 1 (unused), 3.2 MHz blck
DAI_CONFIG(SSP, 1, 0, SSP1-Codec,
	SSP_CONFIG(I2S, SSP_CLOCK(mclk, 19200000, codec_mclk_in),
		SSP_CLOCK(bclk, 3200000, codec_slave),
		SSP_CLOCK(fsync, 50000, codec_slave),
		SSP_TDM(2, 32, 3, 3),
		SSP_CONFIG_DATA(SSP, 1, 32, 1)))

##SSP 1 (ID: 0) with 19.2 MHz mclk with MCLK_ID 1 (unused), 2.4 MHz blck
#DAI_CONFIG(SSP, 1, 0, SSP1-Codec,
#	SSP_CONFIG(I2S, SSP_CLOCK(mclk, 19200000, codec_mclk_in),
#		SSP_CLOCK(bclk, 2400000, codec_slave),
#		SSP_CLOCK(fsync, 50000, codec_slave),
#		SSP_TDM(2, 24, 3, 3),
#		SSP_CONFIG_DATA(SSP, 1, 24, 1)))


##SSP 1 (ID: 0) with 19.2 MHz mclk with MCLK_ID 1 (unused), 1.6 MHz blck
#DAI_CONFIG(SSP, 1, 0, SSP1-Codec,
#	SSP_CONFIG(I2S, SSP_CLOCK(mclk, 19200000, codec_mclk_in),
#		SSP_CLOCK(bclk, 1600000, codec_slave),
#		SSP_CLOCK(fsync, 50000, codec_slave),
#		SSP_TDM(2, 16, 3, 3),
#		SSP_CONFIG_DATA(SSP, 1, 16, 1)))

## remove warnings with SST hard-coded routes

VIRTUAL_WIDGET(ssp1 Tx, out_drv, 0)
