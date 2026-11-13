# Function topology: rt721 SDCA mic for ACP_7_F
#
# PCM      Description             DAI LINK      DAI BE
#  4       SDW DMIC Capture        4             SDW0-PIN16-CAPTURE-SmartMic
#
# PCM4 <---- buffer <---- SDW0-PIN16-CAPTURE

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

define(DI_SDW0_ACP_SW_AUDIO_5_RX,	16)

define(DAI_BE_SDW0_ACP_SW_AUDIO_5_RX,	SDW0-PIN16-CAPTURE-SmartMic)

# PCM 4, SDW DMIC Capture, DAI link id 4, Dai index 16, BE SDW0-PIN16-CAPTURE
PIPELINE_PCM_ADD(sof/pipe-passthrough-capture.m4,
	4, 4, 2, s16le,
	2000, 0, 0,
	48000, 48000, 48000)

DAI_ADD(sof/pipe-dai-capture.m4, 4, ACP_SDW, DI_SDW0_ACP_SW_AUDIO_5_RX, DAI_BE_SDW0_ACP_SW_AUDIO_5_RX,
	PIPELINE_SINK_4, 2, s16le, 2000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

DAI_CONFIG(ACP_SDW, DI_SDW0_ACP_SW_AUDIO_5_RX, 4, DAI_BE_SDW0_ACP_SW_AUDIO_5_RX,
	ACP_SDW_CONFIG(ACP_SDW_CONFIG_DATA(ACP_SDW, DI_SDW0_ACP_SW_AUDIO_5_RX, 48000, 2)))

PCM_CAPTURE_ADD(ACP-SW0-PIN16-CAPTURE-DMIC, 4, PIPELINE_PCM_4)

DEBUG_END
