#Required Topology for rt721 with ACP DMIC Card for ACP_7_X
#
# PCM      Description             DAI LINK      DAI BE
#  0       HS Playback             0             SDW0-PIN0-PLAYBACK-SimpleJack       AUDIO_TX
#  1       HS Capture              1             SDW0-PIN11-CAPTURE-SimpleJack        AUDIO_RX
#  2       Speaker playback        2             SDW0-PIN4-PLAYBACK-SmartAmp         BT_TX
#  4       SDW DMIC                4             SDW0-PIN16-CAPTURE-SmartMic          HS_RX

#
# Define the pipelines
#
# PCM0 ----> buffer ----> AUDIO_TX
# PCM1 <---- buffer <---- AUDIO_RX
# PCM2 ----> buffer ----> BT_TX
# PCM4 <---- buffer <---- HS_RX

# Include topology builder
include(`utils.m4')
include(`dai.m4')
include(`pipeline.m4')
include(`acp-sdw.m4')
include(`acp-dmic.m4')

# Include TLV library
include(`common/tlv.m4')

# Include Token library
include(`sof/tokens.m4')

# Include ACP DSP configuration
include(`platform/amd/acp.m4')

DEBUG_START

#/**********************************************************************************/
# PCM 0, HS Playback, DAI link id 0, Dai index 0(Audio_Tx), BE SW0-PIN0-PLAYBACK

#Driver dai index and dai BE
#DAI Index(Instance * 64 + base_index)      DAI_BE
#0(DI_SDW0_ACP_SW_AUDIO_0_TX)	            SDW0-PIN0-PLAYBACK-SimpleJack
# DI_SDW0_ACP_SW_AUDIO_4_TX                 SDW0-PIN4-PLAYBACK-SmartAmp

define(DI_SDW0_ACP_SW_AUDIO_0_TX,	0)
define(DI_SDW0_ACP_SW_AUDIO_4_TX,	4)
define(DI_SDW0_ACP_SW_AUDIO_0_RX,	11)
define(DI_SDW0_ACP_SW_AUDIO_5_RX,	16)


define(DAI_BE_SDW0_ACP_SW_AUDIO_4_TX,	SDW0-PIN4-PLAYBACK-SmartAmp)
define(DAI_BE_SDW0_ACP_SW_AUDIO_0_TX,	SDW0-PIN0-PLAYBACK-SimpleJack)

define(DAI_BE_SDW0_ACP_SW_AUDIO_0_RX,   SDW0-PIN11-CAPTURE-SimpleJack)
define(DAI_BE_SDW0_ACP_SW_AUDIO_5_RX,   SDW0-PIN16-CAPTURE-SmartMic)

#pipeline: name of the predefined pipeline
#pipe id: pipeline ID. This should be a unique ID identifying the pipeline
#pcm: PCM ID. This will be used to bind to the correct front end DAI link

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate)

dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     deadline, priority, core, time_domain)

dnl DAI_CONFIG(type, dai_index, link_id, name, acphs_config/acpdmic_config)
dnl ACPHS_CONFIG(format, mclk, bclk, fsync, tdm, acphs_config_data)
dnl ACP_CLOCK(clock, freq, codec_provider, polarity)
dnl ACPHS_CONFIG_DATA(type, idx, valid bits, mclk_id)
dnl mclk_id is optional

#/**********************************************************************************/
PIPELINE_PCM_ADD(sof/pipe-passthrough-playback.m4,
	 0, 0, 2, s32le,
	 2000, 0, 0,
     48000, 48000, 48000)

DAI_ADD(sof/pipe-dai-playback.m4, 0, ACP_SDW, DI_SDW0_ACP_SW_AUDIO_0_TX, DAI_BE_SDW0_ACP_SW_AUDIO_0_TX,
PIPELINE_SOURCE_0, 2, s24le, 2000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

DAI_CONFIG(ACP_SDW, DI_SDW0_ACP_SW_AUDIO_0_TX, 0, DAI_BE_SDW0_ACP_SW_AUDIO_0_TX,
	   ACP_SDW_CONFIG(ACP_SDW_CONFIG_DATA(ACP_SDW, DI_SDW0_ACP_SW_AUDIO_0_TX, 48000, 2)))
PCM_PLAYBACK_ADD(ACP-SW0-PIN0-Playback-HS, 0, PIPELINE_PCM_0)

#/**********************************************************************************/
#PCM 2, Speaker Playback, PIPE line 2, DAI link id 2, Dai index 4(DI_SDW0_ACP_SW_AUDIO_4_TX), DAI BE SDW0-PIN4-PLAYBACK-SmartAmp
PIPELINE_PCM_ADD(sof/pipe-passthrough-playback.m4,
	    2, 2, 2, s32le,
 	    2000, 0, 0,
        48000, 48000, 48000)

# playback DAI is ACP soundwire using 2 periods
DAI_ADD(sof/pipe-dai-playback.m4, 2, ACP_SDW, DI_SDW0_ACP_SW_AUDIO_4_TX, DAI_BE_SDW0_ACP_SW_AUDIO_4_TX,
        PIPELINE_SOURCE_2, 2, s24le, 2000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

DAI_CONFIG(ACP_SDW, DI_SDW0_ACP_SW_AUDIO_4_TX, 2, DAI_BE_SDW0_ACP_SW_AUDIO_4_TX,
	    ACP_SDW_CONFIG(ACP_SDW_CONFIG_DATA(ACP_SDW, DI_SDW0_ACP_SW_AUDIO_4_TX, 48000, 2)))

PCM_PLAYBACK_ADD(ACP-SW0-PIN4-Playback-SPK, 2, PIPELINE_PCM_2)
#/**********************************************************************************/

#/**********************************************************************************/
#PCM 1, HS Capture, DAI link id 1, Dai index 11(DI_SDW0_ACP_SW_AUDIO_0_RX), BE DAI_BE_SDW0_ACP_SW_AUDIO_0_RX
# Capture pipeline 1 on PCM 1 using max 2 channels of s24le.
PIPELINE_PCM_ADD(sof/pipe-passthrough-capture.m4,
        1, 1, 2, s32le,
        2000, 0, 0,
        48000, 48000, 48000)

# Capture DAI is ACP soundwire using 2 periods
DAI_ADD(sof/pipe-dai-capture.m4, 1, ACP_SDW, DI_SDW0_ACP_SW_AUDIO_0_RX, DAI_BE_SDW0_ACP_SW_AUDIO_0_RX,
        PIPELINE_SINK_1, 2, s24le, 2000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

DAI_CONFIG(ACP_SDW, DI_SDW0_ACP_SW_AUDIO_0_RX, 1, DAI_BE_SDW0_ACP_SW_AUDIO_0_RX,
	    ACP_SDW_CONFIG(ACP_SDW_CONFIG_DATA(ACP_SDW, DI_SDW0_ACP_SW_AUDIO_0_RX, 48000, 2)))

PCM_CAPTURE_ADD(ACP-SW0-PIN11-Capture-HS, 1, PIPELINE_PCM_1)
#/**********************************************************************************/
#/**********************************************************************************/
#PCM 4, SDW Capture, DAI link id 4, Dai index 16(DI_SDW0_ACP_SW_AUDIO_5_RX), BE SDW0-PIN16-CAPTURE-SmartMic
# Capture pipeline 1 on PCM 1 using max 2 channels of s24le.
PIPELINE_PCM_ADD(sof/pipe-passthrough-capture.m4,
        4, 4, 2, s32le,
        2000, 0, 0,
        48000, 48000, 48000)

# Capture DAI is ACP soundwire using 2 periods
DAI_ADD(sof/pipe-dai-capture.m4, 4, ACP_SDW, DI_SDW0_ACP_SW_AUDIO_5_RX, DAI_BE_SDW0_ACP_SW_AUDIO_5_RX,
        PIPELINE_SINK_4, 2, s24le, 2000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

DAI_CONFIG(ACP_SDW, DI_SDW0_ACP_SW_AUDIO_5_RX, 4, DAI_BE_SDW0_ACP_SW_AUDIO_5_RX,
	    ACP_SDW_CONFIG(ACP_SDW_CONFIG_DATA(ACP_SDW, DI_SDW0_ACP_SW_AUDIO_5_RX, 48000, 2)))

PCM_CAPTURE_ADD(ACP-SW0-PIN16-CAPTURE-DMIC, 4, PIPELINE_PCM_4)

DEBUG_END
