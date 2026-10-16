#Required Topology for rt722 with ACP DMIC Card for ACP7X.
#Required PDM instance and capture rate:
#   -DPDM_INDEX=0  PDM0, PCM name DMIC0, rate 48000 only
#   -DPDM_INDEX=1  PDM1, PCM name DMIC1
#   -DPDM_RATE=48000 for PDM0; 16000|48000|96000 for PDM1
#
# PCM      Description             DAI LINK      DAI BE
#  0       HS Playback             0             SDW0-PIN0-PLAYBACK       AUDIO_TX
#  2       Speaker playback        2             SDW0-PIN4-PLAYBACK       BT_TX
#  3       ACP PDM DMIC            5             acp-dmic-codec
#
# DMIC capture rate. PDM0 is 48000 only. Playback stays 48000.
ifdef(`PDM_RATE', `', `define(`PDM_RATE', 48000)')

# DMIC PDM instance (dai_index). 0 is PDM0, 1 is PDM1.
ifdef(`PDM_INDEX', `', `define(`PDM_INDEX', 0)')
ifelse(PDM_INDEX, 0, `ifelse(PDM_RATE, 48000, `',
`errprint(`PDM0 capture rate must be 48000, got 'PDM_RATE`
')m4exit(1)')')

#
# Define the pipelines
#
# PCM0 ----> buffer ----> AUDIO_TX
# PCM2 ----> buffer ----> BT_TX
# PCM3 <---- buffer <---- acp-dmic-codec

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

define(DI_SDW0_ACP_SW_AUDIO_0_TX,	0 )
define(DI_SDW0_ACP_SW_AUDIO_4_TX,	4 )

define(DAI_BE_SDW0_ACP_SW_AUDIO_4_TX,	SDW0-PIN4-PLAYBACK)
define(DAI_BE_SDW0_ACP_SW_AUDIO_0_TX,	SDW0-PIN0-PLAYBACK)

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
# PCM 0, HS Playback, DAI link id 0, Dai index 0(Audio_Tx), BE SW0-PIN0-PLAYBACK
PIPELINE_PCM_ADD(sof/pipe-passthrough-playback.m4,
	0, 0, 2, s16le,
	2000, 0, 0,
     48000, 48000, 48000)

DAI_ADD(sof/pipe-dai-playback.m4, 0, ACP_SDW, DI_SDW0_ACP_SW_AUDIO_0_TX, DAI_BE_SDW0_ACP_SW_AUDIO_0_TX,
PIPELINE_SOURCE_0, 2, s16le, 2000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

DAI_CONFIG(ACP_SDW, DI_SDW0_ACP_SW_AUDIO_0_TX, 0, DAI_BE_SDW0_ACP_SW_AUDIO_0_TX,
	  ACP_SDW_CONFIG(ACP_SDW_CONFIG_DATA(ACP_SDW, DI_SDW0_ACP_SW_AUDIO_0_TX, 48000, 2)))
PCM_PLAYBACK_ADD(ACP-SW0-PIN0-Playback-HS, 0, PIPELINE_PCM_0)

#/**********************************************************************************/
#PCM 2, Speaker Playback, PIPE line 2, DAI link id 2, Dai index 4(DI_SDW0_ACP_SW_AUDIO_4_TX), DAI BE SDW0-PIN4-PLAYBACK-SmartAmp
PIPELINE_PCM_ADD(sof/pipe-passthrough-playback.m4,
	   2, 2, 2, s16le,
	   2000, 0, 0,
        48000, 48000, 48000)

# playback DAI is ACP soundwire using 2 periods
DAI_ADD(sof/pipe-dai-playback.m4, 2, ACP_SDW, DI_SDW0_ACP_SW_AUDIO_4_TX, DAI_BE_SDW0_ACP_SW_AUDIO_4_TX,
        PIPELINE_SOURCE_2, 2, s16le, 2000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

DAI_CONFIG(ACP_SDW, DI_SDW0_ACP_SW_AUDIO_4_TX, 2, DAI_BE_SDW0_ACP_SW_AUDIO_4_TX,
	   ACP_SDW_CONFIG(ACP_SDW_CONFIG_DATA(ACP_SDW, DI_SDW0_ACP_SW_AUDIO_4_TX, 48000, 2)))

PCM_PLAYBACK_ADD(ACP-SW0-PIN4-Playback-SPK, 2, PIPELINE_PCM_2)
#/**********************************************************************************/
# Capture pipeline 6 on PCM 3 using max 2 channels of s32le.
# DAI index PDM_INDEX (0 = PDM0, 1 = PDM1), BE acp-dmic-codec, DAI link id 5.
PIPELINE_PCM_ADD(sof/pipe-passthrough-capture.m4,
        6, 3, 2, s32le,
        2000, 0, 0,
        PDM_RATE, PDM_RATE, PDM_RATE)

DAI_ADD(sof/pipe-dai-capture.m4,
        6, ACPDMIC, PDM_INDEX, acp-dmic-codec,
        PIPELINE_SINK_6, 2, s32le,
        2000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

#dnl DAI_CONFIG(type, dai_index, link_id, name, acpdmic_config)
DAI_CONFIG(ACPDMIC, PDM_INDEX, 5, acp-dmic-codec,
	   ACPDMIC_CONFIG(ACPDMIC_CONFIG_DATA(ACPDMIC, PDM_INDEX, PDM_RATE, 2)))

#==========================================================================
# PCM id 3. Name follows PDM_INDEX: DMIC0 or DMIC1.
PCM_CAPTURE_ADD(DMIC`'PDM_INDEX, 3, PIPELINE_PCM_6)
#/**********************************************************************************/
DEBUG_END
