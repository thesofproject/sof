#
# Topology for pass through pipeline
#

# Include topology builder
include(`utils.m4')
include(`pipeline.m4')
include(`dai.m4')
include(`ssp.m4')

# Include TLV library
include(`common/tlv.m4')

# Include Token library
include(`sof/tokens.m4')

# Include Apollolake DSP configuration
include(`platform/intel/bxt.m4')

DEBUG_START

dnl Produce uppercase for input string
define(`upcase', `translit(`$*', `a-z', `A-Z')')

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
# TEST_PIPE_AMOUNT - Total amount of pipelines e.g. 1, 2, 3, 4
#

# Define the algorithm configurations blobs to apply such as filter coefficients
include(`test_pipeline_filters.m4')

# Define TEST_HAS_PIPEn flags according to TEST_PIPE_AMOUNT. Those flags will be
# used to determine whether PIPELINE_n should be added.
ifelse(TEST_PIPE_AMOUNT, `2',
`
define(TEST_HAS_PIPE2)
')

ifelse(TEST_PIPE_AMOUNT, `3',
`
define(TEST_HAS_PIPE2)
define(TEST_HAS_PIPE3)
')

ifelse(TEST_PIPE_AMOUNT, `4',
`
define(TEST_HAS_PIPE2)
define(TEST_HAS_PIPE3)
define(TEST_HAS_PIPE4)
')

#
# Define the pipeline(s)
#
# PCM0P --> BUF1.0 --> TEST_PIPE_NAME --> BUF1.1 --> SSP(TEST_DAI_PORT)
ifdef(`TEST_HAS_PIPE2',
`
#                                   +---> BUF2.0 --> SSP(0 or 1)
')
ifdef(`TEST_HAS_PIPE3',
`
#                                   +---> BUF3.0 --> SSP(2 or 3)
')
ifdef(`TEST_HAS_PIPE4',
`
#                                   +---> BUF4.0 --> SSP(4 or 5)
')
#

# Playback pipeline 1 on PCM 0 using max 2 channels of s32le.
# Set 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-TEST_PIPE_NAME-playback.m4,
	1, 0, 2, s32le,
	1000, 0, 0,
	8000, 192000, 48000)

# Generalize the pipeline junction name as PIPELINE_JUNCTION.
# e.g. TEST_PIPE_NAME=`crossover' --> PIPELINE_JUNCTION=`PIPELINE_CROSSOVER_1'
define(PIPELINE_JUNCTION, concat(concat(`PIPELINE_', upcase(TEST_PIPE_NAME)), `_1'))

ifdef(`TEST_HAS_PIPE2',
`
# Playback pipeline 2 on PCM 1 using max 2 channels of s32le.
# Set 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-dai-endpoint.m4,
	2, 1, 2, s32le,
	1000, 0, 0,
	8000, 192000, 48000)

# connect pipelines together
SectionGraph."pipe-sof-second-pipe" {
        index "2"

        lines [
		# connect the second sink buffer
                dapm(PIPELINE_SOURCE_2, PIPELINE_JUNCTION)
        ]
}
')

ifdef(`TEST_HAS_PIPE3',
`
# Playback pipeline 3 on PCM 2 using max 2 channels of s32le.
# Set 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-dai-endpoint.m4,
	3, 2, 2, s32le,
	1000, 0, 0,
	8000, 192000, 48000)

# connect pipelines together
SectionGraph."pipe-sof-third-pipe" {
        index "3"

        lines [
		# connect the third sink buffer
                dapm(PIPELINE_SOURCE_3, PIPELINE_JUNCTION)
        ]
}
')

ifdef(`TEST_HAS_PIPE4',
`
# Playback pipeline 4 on PCM 3 using max 2 channels of s32le.
# Set 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-dai-endpoint.m4,
	4, 3, 2, s32le,
	1000, 0, 0,
	8000, 192000, 48000)

# connect pipelines together
SectionGraph."pipe-sof-fourth-pipe" {
        index "4"

        lines [
		# connect the fourth sink buffer
                dapm(PIPELINE_SOURCE_4, PIPELINE_JUNCTION)
        ]
}
')

# DAI configuration

# Use 3 periods for SRC DAI buffer, otherwise 2 periods
ifelse(TEST_PIPE_NAME, `src', `define(TEST_DAI_PERIODS, `3')', `define(TEST_DAI_PERIODS, `2')')

# playback DAI is SSP TEST_DAI_PORT using TEST_DAI_PERIODS periods
# Buffers use s24le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	1, TEST_DAI_TYPE, TEST_DAI_PORT, TEST_DAI_LINK_NAME,
	PIPELINE_SOURCE_1, TEST_DAI_PERIODS, TEST_DAI_FORMAT,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

ifdef(`TEST_HAS_PIPE2',
`
define(TEST_DAI2_PORT, ifelse(TEST_DAI_PORT, `0', `1', `0'))
define(TEST_DAI2_LINK_NAME, ifelse(TEST_DAI_PORT, `0', `SSP1-Codec', `SSP0-Codec'))

# playback DAI is SSP TEST_DAI2_PORT using 2 periods
# Buffers use s24le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD_SCHED(sof/pipe-dai-sched-playback.m4,
	2, TEST_DAI_TYPE, TEST_DAI2_PORT, TEST_DAI2_LINK_NAME,
	PIPELINE_SOURCE_2, 2, TEST_DAI_FORMAT,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER,
	PIPELINE_PLAYBACK_SCHED_COMP_1)
')

ifdef(`TEST_HAS_PIPE3',
`
define(TEST_DAI3_PORT, ifelse(TEST_DAI_PORT, `2', `3', `2'))
define(TEST_DAI3_LINK_NAME, ifelse(TEST_DAI_PORT, `2', `SSP3-Codec', `SSP2-Codec'))

# playback DAI is SSP TEST_DAI3_PORT using 2 periods
# Buffers use s24le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD_SCHED(sof/pipe-dai-sched-playback.m4,
	3, TEST_DAI_TYPE, TEST_DAI3_PORT, TEST_DAI3_LINK_NAME,
	PIPELINE_SOURCE_3, 2, TEST_DAI_FORMAT,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER,
	PIPELINE_PLAYBACK_SCHED_COMP_1)
')

ifdef(`TEST_HAS_PIPE4',
`
define(TEST_DAI4_PORT, ifelse(TEST_DAI_PORT, `4', `5', `4'))
define(TEST_DAI4_LINK_NAME, ifelse(TEST_DAI_PORT, `4', `SSP5-Codec', `SSP4-Codec'))

# playback DAI is SSP TEST_DAI4_PORT using 2 periods
# Buffers use s24le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD_SCHED(sof/pipe-dai-sched-playback.m4,
	4, TEST_DAI_TYPE, TEST_DAI4_PORT, TEST_DAI4_LINK_NAME,
	PIPELINE_SOURCE_4, 2, TEST_DAI_FORMAT,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER,
	PIPELINE_PLAYBACK_SCHED_COMP_1)
')

# PCM Passthrough
PCM_PLAYBACK_ADD(Passthrough, 0, PIPELINE_PCM_1)

#
# BE configurations - overrides config in ACPI if present
#
# Clocks masters wrt codec
#
# TEST_SSP_DATA_BITS bit I2S
# using TEST_SSP_PHY_BITS bit sample container on SSP port(s)
#
DAI_CONFIG(TEST_DAI_TYPE, TEST_DAI_PORT, 0, TEST_DAI_LINK_NAME,
	   SSP_CONFIG(TEST_SSP_MODE,
		      SSP_CLOCK(mclk, TEST_SSP_MCLK, codec_mclk_in),
		      SSP_CLOCK(bclk, TEST_SSP_BCLK, codec_slave),
		      SSP_CLOCK(fsync, 48000, codec_slave),
		      SSP_TDM(2, TEST_SSP_PHY_BITS, 3, 3),
		      SSP_CONFIG_DATA(TEST_DAI_TYPE, TEST_DAI_PORT,
				      TEST_SSP_DATA_BITS, TEST_SSP_MCLK_ID)))

ifdef(`TEST_HAS_PIPE2',
`
DAI_CONFIG(TEST_DAI_TYPE, TEST_DAI2_PORT, 1, TEST_DAI2_LINK_NAME,
	   SSP_CONFIG(TEST_SSP_MODE,
		      SSP_CLOCK(mclk, TEST_SSP_MCLK, codec_mclk_in),
		      SSP_CLOCK(bclk, TEST_SSP_BCLK, codec_slave),
		      SSP_CLOCK(fsync, 48000, codec_slave),
		      SSP_TDM(2, TEST_SSP_PHY_BITS, 3, 3),
		      SSP_CONFIG_DATA(TEST_DAI_TYPE, TEST_DAI2_PORT,
				      TEST_SSP_DATA_BITS, TEST_SSP_MCLK_ID)))
')

ifdef(`TEST_HAS_PIPE3',
`
DAI_CONFIG(TEST_DAI_TYPE, TEST_DAI3_PORT, 2, TEST_DAI3_LINK_NAME,
	   SSP_CONFIG(TEST_SSP_MODE,
		      SSP_CLOCK(mclk, TEST_SSP_MCLK, codec_mclk_in),
		      SSP_CLOCK(bclk, TEST_SSP_BCLK, codec_slave),
		      SSP_CLOCK(fsync, 48000, codec_slave),
		      SSP_TDM(2, TEST_SSP_PHY_BITS, 3, 3),
		      SSP_CONFIG_DATA(TEST_DAI_TYPE, TEST_DAI3_PORT,
				      TEST_SSP_DATA_BITS, TEST_SSP_MCLK_ID)))
')

ifdef(`TEST_HAS_PIPE4',
`
DAI_CONFIG(TEST_DAI_TYPE, TEST_DAI4_PORT, 3, TEST_DAI4_LINK_NAME,
	   SSP_CONFIG(TEST_SSP_MODE,
		      SSP_CLOCK(mclk, TEST_SSP_MCLK, codec_mclk_in),
		      SSP_CLOCK(bclk, TEST_SSP_BCLK, codec_slave),
		      SSP_CLOCK(fsync, 48000, codec_slave),
		      SSP_TDM(2, TEST_SSP_PHY_BITS, 3, 3),
		      SSP_CONFIG_DATA(TEST_DAI_TYPE, TEST_DAI4_PORT,
				      TEST_SSP_DATA_BITS, TEST_SSP_MCLK_ID)))
')

DEBUG_END
