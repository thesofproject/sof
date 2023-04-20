
#Old Topology
#SDW0-Playback 0
#SDW1-Capture   1
#acp-dmic-codec  2

#
# Topology for Renoir with I2S SP and DMIC.
#

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

#/**********************************************************************************/
PIPELINE_PCM_ADD(sof/pipe-passthrough-playback.m4,
	 1, 1, 2, s16le,
 	2000, 0, 0,
        48000, 48000, 48000)

# playback DAI is ACPSP using 2 periods
DAI_ADD(sof/pipe-dai-playback.m4, 1, ACP_SDW, 0, SDW0-Playback,
PIPELINE_SOURCE_1, 2, s16le, 2000, 0, 0, SCHEDULE_TIME_DOMAIN_DMA)

DAI_CONFIG(ACP_SDW, 1, 0, SDW0-Playback,
	   ACP_SDW_CONFIG(I2S, ACP_CLOCK(mclk, 49152000, codec_mclk_in),
                ACP_CLOCK(bclk, 3072000, codec_slave),
                ACP_CLOCK(fsync, 48000, codec_slave),
                ACP_TDM(2, 32, 3, 3),ACP_SDW_CONFIG_DATA(ACP_SDW, 1, 48000, 2, 0)))
PCM_PLAYBACK_ADD(ACP_SDW_AUDIO0, 1, PIPELINE_PCM_1)
#/**********************************************************************************/

#/**********************************************************************************/
# PCM  id 0

# Capture pipeline 2 on PCM 0 using max 2 channels of s16le.
PIPELINE_PCM_ADD(sof/pipe-passthrough-capture.m4,
        2, 0, 2, s16le,
        2000, 0, 0,
        48000, 48000, 48000)

# Capture DAI is ACPSP using 2 periods
DAI_ADD(sof/pipe-dai-capture.m4, 2, ACP_SDW, 1, SDW1-Capture,
PIPELINE_SINK_2, 2, s16le, 2000, 0, 0, SCHEDULE_TIME_DOMAIN_DMA)

DAI_CONFIG(ACP_SDW, 1, 1, SDW1-Capture,
	   ACP_SDW_CONFIG(I2S, ACP_CLOCK(mclk, 49152000, codec_mclk_in),
                ACP_CLOCK(bclk, 3072000, codec_slave),
                ACP_CLOCK(fsync, 48000, codec_slave),
                ACP_TDM(2, 32, 3, 3),ACP_SDW_CONFIG_DATA(ACP_SDW, 1, 48000, 2, 0)))
PCM_CAPTURE_ADD(ACP_SDW_AUDIO0, 0, PIPELINE_PCM_2)
#/**********************************************************************************/

#/**********************************************************************************/
# Capture pipeline 3 on PCM 1 using max 2 channels of s32le.
PIPELINE_PCM_ADD(sof/pipe-passthrough-capture.m4,
        3, 4, 2, s32le,
        2000, 0, 0,
        48000, 48000, 48000)

DAI_ADD(sof/pipe-dai-capture.m4, 3, ACPDMIC, 0, acp-dmic-codec,
PIPELINE_SINK_3, 2, s32le, 2000, 0, 0, SCHEDULE_TIME_DOMAIN_DMA)

dnl DAI_CONFIG(type, dai_index, link_id, name, acpdmic_config)
DAI_CONFIG(ACPDMIC, 3, 2, acp-dmic-codec,
	   ACPDMIC_CONFIG(ACPDMIC_CONFIG_DATA(ACPDMIC, 3, 48000, 2)))

# PCM id 1
PCM_CAPTURE_ADD(DMIC, 4, PIPELINE_PCM_3)
#/**********************************************************************************/
