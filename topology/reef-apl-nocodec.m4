#
# Topology for generic Apollolake board with no codec.
#

# Include topology builder
include(`local.m4')
include(`build.m4')

# Include TLV library
include(`common/tlv.m4')

# Include Token library
include(`sof/tokens.m4')

# Include Apollolake DSP configuration
include(`dsps/bxt.m4')

#
# Define the pipelines
#
# PCM0 ----> volume ---------------+
#                                  |--low latency mixer ----> volume ---->  SSP2
# PCM2 ----> SRC -----> volume ----+
#                                  |
# PCM3 ----> SRC -----> volume ----+
#                                  |
#           Tone -----> volume ----+
#
# PCM1 <---- Volume <---- SSP2
#

# Low Latency playback pipeline 1 on PCM 0 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
# Use DMAC 0 channel 1 for PCM audio playback data
PIPELINE_PCM_ADD(sof/pipe-low-latency-playback.m4, 1, 0, 2, s32le, 48, 1000, 0, 0, 0, 1)

# Low Latency capture pipeline 2 on PCM 1 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
# Use DMAC 0 channel 2 for PCM audio capture data
PIPELINE_PCM_ADD(sof/pipe-low-latency-capture.m4, 2, 1, 2, s32le, 48, 1000, 0, 0, 0, 2)

# PCM Media Playback pipeline 3 on PCM 2 using max 2 channels of s32le.
# Schedule 96 frames per 2000us deadline on core 0 with priority 1
# Use DMAC 0 channel 3 for PCM audio playback data
PIPELINE_PCM_ADD(sof/pipe-pcm-media.m4, 3, 2, 2, s32le, 96, 2000, 1, 0, 0, 3)

# PCM Media Playback pipeline 4 on PCM 3 using max 2 channels of s32le.
# Schedule 96 frames per 2000us deadline on core 0 with priority 1
# Use DMAC 0 channel 4 for PCM audio playback data
PIPELINE_PCM_ADD(sof/pipe-pcm-media.m4, 4, 3, 2, s32le, 96, 2000, 1, 0, 0, 4)

# Tone Playback pipeline 5 using max 2 channels of s32le.
# Schedule 192 frames per 4000us deadline on core 0 with priority 2
PIPELINE_ADD(sof/pipe-tone.m4, 5, 2, s32le, 192, 4000, 2, 0)

# Connect pipelines together
SectionGraph."pipe-apl-nocodec" {
	index "0"

	lines [
		# media 0
		dapm(PIPELINE_MIXER_1, PIPELINE_SOURCE_3)
		# media 1
		dapm(PIPELINE_MIXER_1, PIPELINE_SOURCE_4)
		#tone
		dapm(PIPELINE_MIXER_1, PIPELINE_SOURCE_5)
	]
}

#
# DAI configuration
#
# SSP port 2 is our only pipeline DAI
#

# playback DAI is SSP2 using I2S DAPM stream and 2 periods
DAI_ADD(sof/pipe-dai-playback.m4, SSP, 2, I2S, PIPELINE_SOURCE_1, 2)

# capture DAI is SSP2 using I2S DAPM stream and 2 periods
DAI_ADD(sof/pipe-dai-capture.m4, SSP, 2, I2S, PIPELINE_SINK_2, 2)

#
# BE configurations - overrides config in ACPI if present
#
DAI_CONFIG(SSP, 2, NoCodec, I2S, I2S, 24,
	DAI_CLOCK(mclk, 19200000, slave),
	DAI_CLOCK(bclk, 2400000, slave),
	DAI_CLOCK(fsync, 48000, slave),
	DAI_TDM(2, 25, 3, 3))
