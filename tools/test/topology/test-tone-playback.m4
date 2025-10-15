#
# Topology for Tone Generator Playback
#

# Include topology builder
include(`pipeline.m4')
include(`dai.m4')
include(`ssp.m4')
include(`utils.m4')

# Include TLV library
include(`common/tlv.m4')

# Include Token library
include(`sof/tokens.m4')

# Include Baytrail DSP configuration
include(`byt.m4')

#
# Machine Specific Config - !! MUST BE SET TO MATCH TEST MACHINE DRIVER !!
#
# TEST_PIPE_NAME - Pipe name
# TEST_DAI_LINK_NAME - BE DAI link name e.g. "NoCodec"
# TEST_DAI_PORT	- SSP port number e.g. 2
# TEST_DAI_FORMAT - SSP data format e.g s16le
# TEST_PIPE_FORMAT - Pipeline format e.g. s16le
# TEST_SSP_MCLK - SSP MCLK in Hz
# TEST_SSP_BCLK - SSP BCLK in Hz
# TEST_SSP_PHY_BITS - SSP physical slot size
# TEST_SSP_DATA_BITS - SSP data slot size
#

#
# Define the pipeline
#
#  Tone --B0--> volume --B1--> SSP2
#

# Set sample rate for tone
define(`TONE_SAMPLE_RATE', 48000)

# Tone Playback pipeline 5 using max 2 channels of TEST_PIPE_FORMAT.
# Schedule with 48 frame per 1000us deadline on core 0 with priority 0
PIPELINE_ADD(sof/pipe-tone.m4,
	5, 2, TEST_PIPE_FORMAT,
	1000, 0, 0, 0, 0,
	48000, 48000, 48000)

# playback DAI is SSP2 using 2 periods
# Buffers use TEST_DAI_FORMAT format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	5, TEST_DAI_TYPE, TEST_DAI_PORT, TEST_DAI_LINK_NAME,
	PIPELINE_SOURCE_5, 2, TEST_DAI_FORMAT,
	1000, 2, 0)

#
# BE configurations - overrides config in ACPI if present
#
# Clocks masters wrt codec
#
# TEST_SSP_DATA_BITS bit I2S using TEST_SSP_PHY_BITS bit sample conatiner on SSP TEST_DAI_PORT
#
DAI_CONFIG(TEST_DAI_TYPE, TEST_DAI_PORT, TEST_DAI_PORT, TEST_DAI_LINK_NAME,
	   SSP_CONFIG(TEST_SSP_MODE,
		      SSP_CLOCK(mclk, TEST_SSP_MCLK, codec_mclk_in),
		      SSP_CLOCK(bclk, TEST_SSP_BCLK, codec_consumer),
		      SSP_CLOCK(fsync, 48000, codec_consumer),
		      SSP_TDM(2, TEST_SSP_PHY_BITS, 3, 3),
		      SSP_CONFIG_DATA(TEST_DAI_TYPE, TEST_DAI_PORT,
				      TEST_SSP_DATA_BITS, TEST_SSP_MCLK_ID)))
