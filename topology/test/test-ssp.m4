#
# Topology for pass through pipeline
#

# Include topology builder
include(`dai.m4')
include(`utils.m4')
include(`pipeline.m4')

# Include TLV library
include(`common/tlv.m4')

# Include Token library
include(`sof/tokens.m4')

# Include Baytrail DSP configuration
include(`dsps/byt.m4')

#
# Machine Specific Config - !! MUST BE SET TO MATCH TEST MACHINE DRIVER !!
#
# TEST_PIPE_NAME - Pipe name
# TEST_DAI_LINK_NAME - BE DAI link name e.g. "NoCodec"
# TEST_DAI_PORT	- SSP port number e.g. 2
# TEST_DAI_FORMAT - SSP data format e.g s16le
# TEST_PIPE_FORMAT - Pipeline format e.g. s16le
# TEST_SSP_MCLK - SSP BCLK in Hz
# TEST_SSP_BCLK - SSP BCLK in Hz
# TEST_SSP_PHY_BITS - SSP physical slot size
# TEST_SSP_DATA_BITS - SSP data slot size
# TEST_SSP_MODE - SSP mode e.g. I2S, LEFT_J, DSP_A and DSP_B
#

#
# Define the pipeline
#
# PCM0 <-- TEST_PIPE_NAME pipe --> SSP TEST_DAI_PORT
#

# Passthrough playback pipeline 1 on PCM 0 using max 2 channels of s24le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
# Use DMAC 0 channel 1 for PCM audio playback data

PIPELINE_PCM_DAI_ADD(sof/pipe-TEST_PIPE_NAME-playback.m4,
	1, 0, 2, TEST_PIPE_FORMAT,
	48, 1000, 0, 0, 0, 1,
	TEST_DAI_TYPE, TEST_DAI_PORT, TEST_DAI_FORMAT, 2)


# Passthrough playback pipeline 2 on PCM 0 using max 2 channels of s24le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
# Use DMAC 0 channel 1 for PCM audio playback data

PIPELINE_PCM_DAI_ADD(sof/pipe-TEST_PIPE_NAME-capture.m4,
	2, 0, 2, TEST_PIPE_FORMAT,
	48, 1000, 0, 0, 0, 1,
	TEST_DAI_TYPE, TEST_DAI_PORT, TEST_DAI_FORMAT, 2)

#
# DAI configuration
#
# SSP port TEST_DAI_PORT is our only pipeline DAI
#

# playback DAI is SSP TEST_DAI_PORT using 2 periods
# Buffers use s24le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	1, TEST_DAI_TYPE, TEST_DAI_PORT, TEST_DAI_LINK_NAME,
	PIPELINE_SOURCE_1, 2, TEST_DAI_FORMAT,
	48, 1000, 0, 0)

# capture DAI is SSP TEST_DAI_PORT using 2 periods
# Buffers use s24le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	2, TEST_DAI_TYPE, TEST_DAI_PORT, TEST_DAI_LINK_NAME,
	PIPELINE_SINK_2, 2, TEST_DAI_FORMAT,
	48, 1000, 0, 0)

# PCM Passthrough
PCM_DUPLEX_ADD(Passthrough, 3, 0, 0, PIPELINE_PCM_1, PIPELINE_PCM_2)

#
# BE configurations - overrides config in ACPI if present
#
# Clocks masters wrt codec
#
# TEST_SSP_DATA_BITS bit I2S using TEST_SSP_PHY_BITS bit sample conatiner on SSP TEST_DAI_PORT
#
DAI_CONFIG(SSP, TEST_SSP_PORT, 0, TEST_DAI_LINK_NAME, TEST_SSP_MODE, TEST_SSP_DATA_BITS,
	DAI_CLOCK(mclk, TEST_SSP_MCLK, codec_mclk_in),
	DAI_CLOCK(bclk, TEST_SSP_BCLK, codec_slave),
	DAI_CLOCK(fsync, 48000, codec_slave),
	DAI_TDM(2, TEST_SSP_PHY_BITS, 3, 3))
