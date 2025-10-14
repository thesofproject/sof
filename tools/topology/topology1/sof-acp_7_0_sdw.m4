#Required Topology for rt722 with ACP DMIC Card for ACP_7_0
#
# PCM      Description             DAI LINK      DAI BE
#  0       HS Playback             0             SDW0-PIN0-Playback-SimpleJack       AUDIO_TX
#  1       HS Capture              1             SDW0-PIN3-Capture-SimpleJack        AUDIO_RX
#  2       Speaker playback        2             SDW0-PIN1-Playback-SmartAmp         BT_TX
#  4       SDW DMIC                4             SDW0-PIN5-Capture-SmartMic          HS_RX

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
#0(AUDIO_TX)	                            SDW0-PIN0-PLAYBACK-SimpleJack
#1(BT_TX)	                                SDW0-PIN1-PLAYBACK-SmartAmp
#2(HS_TX)	                                SDW0-PIN2-PLAYBACK
#3(AUDIO_RX)	                            SDW0-PIN3-CAPTURE-SimpleJack
#4(BT_RX)	                                SDW0-PIN4-CAPTURE-SmartAmp
#5(HS_RX)	                                SDW0-PIN5-CAPTURE

define(DI_SDW0_ACP_SW_Audio_TX, 0)
define(DI_SDW0_ACP_SW_BT_TX, 1)
define(DI_SDW0_ACP_SW_HS_TX, 2)
define(DI_SDW0_ACP_SW_Audio_RX, 3)
define(DI_SDW0_ACP_SW_BT_RX, 4)
define(DI_SDW0_ACP_SW_HS_RX, 5)
define(DI_SDW1_ACP_P1_SW_BT_RX, 68)
define(DI_SDW1_ACP_P1_SW_BT_TX, 65)

define(DAI_BE_SDW0_ACP_SW_HS_TX, SDW0-PIN2-PLAYBACK)
define(DAI_BE_SDW1_ACP_P1_SW_BT_RX, SDW1-PIN1-CAPTURE-SmartMic)
define(DAI_BE_SDW1_ACP_P1_SW_BT_TX, SDW1-PIN1-PLAYBACK)
define(DAI_BE_ACP_SW_Audio_RX, SDW0-PIN3-CAPTURE-SimpleJack)
define(DAI_BE_SDW0_ACP_SW_Audio_TX, SDW0-PIN0-PLAYBACK-SimpleJack)
define(DAI_BE_SDW0_ACP_SW_BT_RX, SDW0-PIN4-CAPTURE-SmartAmp)
define(DAI_BE_SDW0_ACP_SW_BT_TX, SDW0-PIN1-PLAYBACK-SmartAmp)
define(DAI_BE_SDW0_ACP_SW_HS_RX, SDW0-PIN5-CAPTURE-SmartMic)

#pipeline: name of the predefined pipeline
#pipe id: pipeline ID. This should be a unique ID identifying the pipeline
#pcm: PCM ID. This will be used to bind to the correct front end DAI link

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate)
PIPELINE_PCM_ADD(sof/pipe-passthrough-playback.m4,
	    0, 0, 2, s16le,
 	    2000, 0, 0,
        48000, 48000, 48000)

dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     deadline, priority, core, time_domain)
DAI_ADD(sof/pipe-dai-playback.m4, 0, ACP_SDW, DI_SDW0_ACP_SW_Audio_TX, DAI_BE_SDW0_ACP_SW_Audio_TX,
        PIPELINE_SOURCE_0, 2, s16le, 2000, 0, 0, SCHEDULE_TIME_DOMAIN_DMA)

dnl DAI_CONFIG(type, dai_index, link_id, name, acphs_config/acpdmic_config)
dnl ACPHS_CONFIG(format, mclk, bclk, fsync, tdm, acphs_config_data)
dnl ACP_CLOCK(clock, freq, codec_provider, polarity)
dnl ACPHS_CONFIG_DATA(type, idx, valid bits, mclk_id)
dnl mclk_id is optional
DAI_CONFIG(ACP_SDW, DI_SDW0_ACP_SW_Audio_TX, 0, DAI_BE_SDW0_ACP_SW_Audio_TX,
	    ACP_SDW_CONFIG(ACP_SDW_CONFIG_DATA(ACP_SDW, DI_SDW0_ACP_SW_Audio_TX, 48000, 2)))

PCM_PLAYBACK_ADD(ACP-SW0-PIN0-Playback-HS, 0, PIPELINE_PCM_0)
#/**********************************************************************************/


#/**********************************************************************************/
#PCM 1, HS Capture, DAI link id 1, Dai index 3(Audio_RX), BE SW0-PIN0-CAPTURE
# Capture pipeline 1 on PCM 1 using max 2 channels of s16le.
PIPELINE_PCM_ADD(sof/pipe-passthrough-capture.m4,
        1, 1, 2, s16le,
        2000, 0, 0,
        48000, 48000, 48000)

# Capture DAI is ACP soundwire using 2 periods
DAI_ADD(sof/pipe-dai-capture.m4, 1, ACP_SDW, DI_SDW0_ACP_SW_Audio_RX, DAI_BE_ACP_SW_Audio_RX,
        PIPELINE_SINK_1, 2, s16le, 2000, 0, 0, SCHEDULE_TIME_DOMAIN_DMA)

DAI_CONFIG(ACP_SDW, DI_SDW0_ACP_SW_Audio_RX, 1, DAI_BE_ACP_SW_Audio_RX,
	    ACP_SDW_CONFIG(ACP_SDW_CONFIG_DATA(ACP_SDW, DI_SDW0_ACP_SW_Audio_RX, 48000, 2)))

PCM_CAPTURE_ADD(ACP-SW0-PIN0-Capture-HS, 1, PIPELINE_PCM_1)
#/**********************************************************************************/

#/**********************************************************************************/
#PCM 2, Speaker Playback, DAI link id 2, Dai index 1(BT_TX), BE SW0-PIN1-PLAYBACK
PIPELINE_PCM_ADD(sof/pipe-passthrough-playback.m4,
	    2, 2, 2, s16le,
 	    2000, 0, 0,
        48000, 48000, 48000)

# playback DAI is ACP soundwire using 2 periods
DAI_ADD(sof/pipe-dai-playback.m4, 2, ACP_SDW, DI_SDW0_ACP_SW_BT_TX, DAI_BE_SDW0_ACP_SW_BT_TX,
        PIPELINE_SOURCE_2, 2, s16le, 2000, 0, 0, SCHEDULE_TIME_DOMAIN_DMA)

DAI_CONFIG(ACP_SDW, DI_SDW0_ACP_SW_BT_TX, 2, DAI_BE_SDW0_ACP_SW_BT_TX,
	    ACP_SDW_CONFIG(ACP_SDW_CONFIG_DATA(ACP_SDW, DI_SDW0_ACP_SW_BT_TX, 48000, 2)))

PCM_PLAYBACK_ADD(ACP-SW0-PIN2-Playback-SPK, 2, PIPELINE_PCM_2)
#/**********************************************************************************/

#/**********************************************************************************/
#PCM 4, SDW Capture, DAI link id 4, Dai index 5(HS_RX), BE SDW0-PIN5-CAPTURE-SmartMic
# Capture pipeline 1 on PCM 1 using max 2 channels of s16le.
PIPELINE_PCM_ADD(sof/pipe-passthrough-capture.m4,
        4, 4, 2, s16le,
        2000, 0, 0,
        48000, 48000, 48000)

# Capture DAI is ACP soundwire using 2 periods
DAI_ADD(sof/pipe-dai-capture.m4, 4, ACP_SDW, DI_SDW0_ACP_SW_HS_RX, DAI_BE_SDW0_ACP_SW_HS_RX,
        PIPELINE_SINK_4, 2, s16le, 2000, 0, 0, SCHEDULE_TIME_DOMAIN_DMA)

DAI_CONFIG(ACP_SDW, DI_SDW0_ACP_SW_HS_RX, 4, DAI_BE_SDW0_ACP_SW_HS_RX,
	    ACP_SDW_CONFIG(ACP_SDW_CONFIG_DATA(ACP_SDW, DI_SDW0_ACP_SW_HS_RX, 48000, 2)))

PCM_CAPTURE_ADD(ACP-SW0-PIN5-CAPTURE-DMIC, 4, PIPELINE_PCM_4)
#/**********************************************************************************/

DEBUG_END
