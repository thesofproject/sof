#
# Topology for i.MX8QM / i.MX8QXP boards with cs42888 codec demonstrating mixer component
#

# Include topology builder
include(`utils.m4')
include(`dai.m4')
include(`pipeline.m4')
include(`esai.m4')

# Include TLV library
include(`common/tlv.m4')

# Include Token library
include(`sof/tokens.m4')

# Include DSP configuration
include(`platform/imx/imx8.m4')

#
# Define the pipelines
#
# PCM0 -----> volume -------v
#                            low latency mixer ----> volume ----> ESAI0
# PCM1 -----> volume -------^
# PCM0 <---- Volume <---- ESAI0
#

# Low Latency capture pipeline 2 on PCM 0 using max 2 channels of s24le.
# 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-low-latency-capture.m4,
	2, 0, 2, s24le,
	1000, 0, 0,
	48000, 48000, 48000)

#
# DAI configuration
#
# ESAI port 0 is our only pipeline DAI
#

# playback DAI is ESAI0 using 2 periods
# Buffers use s24le format, 1000us deadline on core 0 with priority 1
# this defines pipeline 1. The 'NOT_USED_IGNORED' is due to dependencies
# and is adjusted later with an explicit dapm line.
DAI_ADD(sof/pipe-mixer-dai-playback.m4,
	1, ESAI, 0, esai0-cs42888,
	NOT_USED_IGNORED, 2, s24le,
	1000, 1, 0, SCHEDULE_TIME_DOMAIN_DMA,
	2, 48000)

# PCM Playback pipeline 3 on PCM 0 using max 2 channels of s24le.
# 1000us deadline on core 0 with priority 0
# this is connected to pipeline DAI 1
PIPELINE_PCM_ADD(sof/pipe-host-volume-playback.m4,
	3, 0, 2, s24le,
	1000, 0, 0,
	48000, 48000, 48000,
	SCHEDULE_TIME_DOMAIN_DMA,
	PIPELINE_PLAYBACK_SCHED_COMP_1)

# PCM Playback pipeline 4 on PCM 1 using max 2 channels of s24le.
# 5ms deadline on core 0 with priority 0
# this is connected to pipeline DAI 1
PIPELINE_PCM_ADD(sof/pipe-host-volume-playback.m4,
	4, 1, 2, s24le,
	5000, 0, 0,
	48000, 48000, 48000,
	SCHEDULE_TIME_DOMAIN_DMA,
	PIPELINE_PLAYBACK_SCHED_COMP_1)

# Connect pipelines together
SectionGraph."PIPE_NAME" {
	index "0"

	lines [
		# PCM pipeline 3 to DAI pipeline 1
		dapm(PIPELINE_MIXER_1, PIPELINE_SOURCE_3)
		# PCM pipeline 4 to DAI pipeline 1
		dapm(PIPELINE_MIXER_1, PIPELINE_SOURCE_4)

	]
}

# capture DAI is ESAI0 using 2 periods
# Buffers use s24le format, 1000us deadline on core 0 with priority 0
# this is part of pipeline 2
DAI_ADD(sof/pipe-dai-capture.m4,
	2, ESAI, 0, esai0-cs42888,
	PIPELINE_SINK_2, 2, s24le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_DMA)


# PCM definitions
PCM_DUPLEX_ADD(PCM, 0, PIPELINE_PCM_3, PIPELINE_PCM_2)
PCM_PLAYBACK_ADD(PCM Deep Buffer, 1, PIPELINE_PCM_4)

#
# BE configurations
#
DAI_CONFIG(ESAI, 0, 0, esai0-cs42888,
	   ESAI_CONFIG(I2S, ESAI_CLOCK(mclk, 49152000, codec_mclk_in),
		      ESAI_CLOCK(bclk, 3072000, codec_slave),
		      ESAI_CLOCK(fsync, 48000, codec_slave),
		      ESAI_TDM(2, 32, 3, 3),
		      ESAI_CONFIG_DATA(ESAI, 0, 0)))
