# Function topology: SDCA jack on SDW1 for ACP_7_F
#
# PCM      Description             DAI LINK      DAI BE
#  0       HS Playback             0             SDW1-PIN0-PLAYBACK-SimpleJack
#  1       HS Capture              1             SDW1-PIN11-CAPTURE-SimpleJack
#
# PCM0 ----> buffer ----> SDW1-PIN0-PLAYBACK
# PCM1 <---- buffer <---- SDW1-PIN11-CAPTURE

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

define(DI_SDW1_ACP_SW_AUDIO_0_TX,	64)
define(DI_SDW1_ACP_SW_AUDIO_0_RX,	75)

define(DAI_BE_SDW1_ACP_SW_AUDIO_0_TX,	SDW1-PIN0-PLAYBACK-SimpleJack)
define(DAI_BE_SDW1_ACP_SW_AUDIO_0_RX,	SDW1-PIN11-CAPTURE-SimpleJack)

# PCM 0, HS Playback, DAI link id 0, Dai index 64, BE SDW1-PIN0-PLAYBACK
PIPELINE_PCM_ADD(sof/pipe-passthrough-playback.m4,
	0, 0, 2, s16le,
	2000, 0, 0,
	48000, 48000, 48000)

DAI_ADD(sof/pipe-dai-playback.m4, 0, ACP_SDW, DI_SDW1_ACP_SW_AUDIO_0_TX, DAI_BE_SDW1_ACP_SW_AUDIO_0_TX,
	PIPELINE_SOURCE_0, 2, s16le, 2000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

DAI_CONFIG(ACP_SDW, DI_SDW1_ACP_SW_AUDIO_0_TX, 0, DAI_BE_SDW1_ACP_SW_AUDIO_0_TX,
	ACP_SDW_CONFIG(ACP_SDW_CONFIG_DATA(ACP_SDW, DI_SDW1_ACP_SW_AUDIO_0_TX, 48000, 2)))

PCM_PLAYBACK_ADD(ACP-SW1-PIN0-Playback-HS, 0, PIPELINE_PCM_0)

# PCM 1, HS Capture, DAI link id 1, Dai index 75, BE SDW1-PIN11-CAPTURE
PIPELINE_PCM_ADD(sof/pipe-passthrough-capture.m4,
	1, 1, 2, s16le,
	2000, 0, 0,
	48000, 48000, 48000)

DAI_ADD(sof/pipe-dai-capture.m4, 1, ACP_SDW, DI_SDW1_ACP_SW_AUDIO_0_RX, DAI_BE_SDW1_ACP_SW_AUDIO_0_RX,
	PIPELINE_SINK_1, 2, s16le, 2000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

DAI_CONFIG(ACP_SDW, DI_SDW1_ACP_SW_AUDIO_0_RX, 1, DAI_BE_SDW1_ACP_SW_AUDIO_0_RX,
	ACP_SDW_CONFIG(ACP_SDW_CONFIG_DATA(ACP_SDW, DI_SDW1_ACP_SW_AUDIO_0_RX, 48000, 2)))

PCM_CAPTURE_ADD(ACP-SW1-PIN11-Capture-HS, 1, PIPELINE_PCM_1)

DEBUG_END
