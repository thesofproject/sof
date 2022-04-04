#
# Topology for pass through pipeline
#

# Include topology builder
include(`utils.m4')
include(`dai.m4')
include(`ssp.m4')
include(`dmic.m4')
include(`pipeline.m4')

# Include TLV library
include(`common/tlv.m4')

# Include Token library
include(`sof/tokens.m4')

# Include Apollolake DSP configuration
include(`platform/intel/bxt.m4')

DEBUG_START

# Define the algorithm configurations blobs to apply such as filter coefficients
include(`test_pipeline_filters.m4')

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
# TEST_SSP_MODE - SSP mode e.g. I2S, LEFT_J, DSP_A and DSP_B
#

#
# Define the pipeline
#
# PCM0C  <--  TEST_PIPE_NAME  <--  SSP TEST_DAI_PORT
#

# Capture pipeline 2 on PCM 0 using max 2 channels of s32le.
# 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-TEST_PIPE_NAME-capture.m4,
	2, 0, 2, s32le,
	1000, 0, 0,
	8000, 192000, 48000)

#
# DAI configuration
#
# SSP port TEST_DAI_PORT is our only pipeline DAI

# Use 3 periods for SRC DAI buffer, otherwise 2 periods
ifelse(TEST_PIPE_NAME, `src', `define(TEST_DAI_PERIODS, `3')', `define(TEST_DAI_PERIODS, `2')')

# capture DAI is SSP TEST_DAI_PORT using TEST_DAI_PERIODS periods
# schedule 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	2, TEST_DAI_TYPE, TEST_DAI_PORT, TEST_DAI_LINK_NAME,
	PIPELINE_SINK_2, TEST_DAI_PERIODS, TEST_DAI_FORMAT,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# PCM Passthrough
PCM_CAPTURE_ADD(Passthrough, 0, PIPELINE_PCM_2)

#
# BE configurations - overrides config in ACPI if present
#
# Clocks masters wrt codec
#
# TEST_SSP_DATA_BITS bit I2S
# using TEST_SSP_PHY_BITS bit sample container on SSP TEST_DAI_PORT
#
DAI_CONFIG(TEST_DAI_TYPE, TEST_DAI_PORT, 0, TEST_DAI_LINK_NAME,
	   ifelse(TEST_DAI_TYPE, `SSP',
		  SSP_CONFIG(TEST_SSP_MODE,
			     SSP_CLOCK(mclk, TEST_SSP_MCLK, codec_mclk_in),
			     SSP_CLOCK(bclk, TEST_SSP_BCLK, codec_slave),
			     SSP_CLOCK(fsync, 48000, codec_slave),
			     SSP_TDM(2, TEST_SSP_PHY_BITS, 3, 3),
			     SSP_CONFIG_DATA(TEST_DAI_TYPE, TEST_DAI_PORT,
					     TEST_SSP_DATA_BITS, TEST_SSP_MCLK_ID)),
		  TEST_DAI_TYPE, `DMIC',
		  DMIC_CONFIG(TEST_DMIC_DRIVER_VERSION, TEST_DMIC_CLK_MIN,
			      TEST_DMIC_CLK_MAX, TEST_DMIC_DUTY_MIN,
			      TEST_DMIC_DUTY_MAX, TEST_DMIC_SAMPLE_RATE,
			      DMIC_WORD_LENGTH(TEST_DAI_FORMAT), TEST_DMIC_UNMUTE_TIME,
			      TEST_DAI_TYPE, TEST_DAI_PORT,
			      PDM_CONFIG(TEST_DAI_TYPE, TEST_DAI_PORT,
					 TEST_DMIC_PDM_CONFIG)),
		  `'))

DEBUG_END
