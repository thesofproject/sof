# Function topology: rt1320 SDCA amp (1 link) for ACP_7_F
#
# PCM      Description             DAI LINK      DAI BE
#  2       Speaker Playback        2             SDW1-PIN4-PLAYBACK-SmartAmp
#
# PCM2 ----> buffer ----> SDW1-PIN4-PLAYBACK

# Include topology builder
include(`utils.m4')
include(`dai.m4')
include(`pipeline.m4')
include(`acp-sdw.m4')

# Include TLV library
include(`common/tlv.m4')

# Include Token library
include(`sof/tokens.m4')

# Include ACP DSP configuration
include(`platform/amd/acp.m4')

DEBUG_START

define(DI_SDW1_ACP_SW_AUDIO_4_TX,	68)

define(DAI_BE_SDW1_ACP_SW_AUDIO_4_TX,	SDW1-PIN4-PLAYBACK-SmartAmp)

# PCM 2, Speaker Playback, DAI link id 2, Dai index 68, BE SDW1-PIN4-PLAYBACK
PIPELINE_PCM_ADD(sof/pipe-passthrough-playback.m4,
	2, 2, 2, s16le,
	2000, 0, 0,
	48000, 48000, 48000)

DAI_ADD(sof/pipe-dai-playback.m4, 2, ACP_SDW, DI_SDW1_ACP_SW_AUDIO_4_TX, DAI_BE_SDW1_ACP_SW_AUDIO_4_TX,
	PIPELINE_SOURCE_2, 2, s16le, 2000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

DAI_CONFIG(ACP_SDW, DI_SDW1_ACP_SW_AUDIO_4_TX, 2, DAI_BE_SDW1_ACP_SW_AUDIO_4_TX,
	ACP_SDW_CONFIG(ACP_SDW_CONFIG_DATA(ACP_SDW, DI_SDW1_ACP_SW_AUDIO_4_TX, 48000, 2)))

PCM_PLAYBACK_ADD(ACP-SW1-PIN4-Playback-SPK, 2, PIPELINE_PCM_2)

DEBUG_END
