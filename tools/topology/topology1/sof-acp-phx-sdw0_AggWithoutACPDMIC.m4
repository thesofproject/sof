
#Required Topology for RTK Monolithic Aggregated without ACP DMIC Card
#1        SW0-PIN0-Playback    AUDIO_TX
#3        SW0-PIN2-Playback    HS_TX
#8        SW1-PIN1-Capture    P1_BT_RX


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
DAI_ADD(sof/pipe-dai-playback.m4, 1, ACP_SDW, 1, SW0-PIN0-Playback,
PIPELINE_SOURCE_1, 2, s16le, 2000, 0, 0, SCHEDULE_TIME_DOMAIN_DMA)

DAI_CONFIG(ACP_SDW, 1, 1, SW0-PIN0-Playback,
	   ACP_SDW_CONFIG(I2S, ACP_CLOCK(mclk, 49152000, codec_mclk_in),
                ACP_CLOCK(bclk, 3072000, codec_slave),
                ACP_CLOCK(fsync, 48000, codec_slave),
                ACP_TDM(2, 32, 3, 3),ACP_SDW_CONFIG_DATA(ACP_SDW, 1, 48000, 2, 0)))
PCM_PLAYBACK_ADD(ACP-SW0-PIN0-Playback, 1, PIPELINE_PCM_1)
#/**********************************************************************************/

#/**********************************************************************************/
# PCM  id 0

# Capture pipeline 2 on PCM 0 using max 2 channels of s16le.
PIPELINE_PCM_ADD(sof/pipe-passthrough-capture.m4,
        2, 0, 2, s16le,
        2000, 0, 0,
        48000, 48000, 48000)

# Capture DAI is ACPSP using 2 periods
DAI_ADD(sof/pipe-dai-capture.m4, 2, ACP_SDW, 7, SW1-PIN1-Capture,
PIPELINE_SINK_2, 2, s16le, 2000, 0, 0, SCHEDULE_TIME_DOMAIN_DMA)

DAI_CONFIG(ACP_SDW, 1, 8, SW1-PIN1-Capture,
	   ACP_SDW_CONFIG(I2S, ACP_CLOCK(mclk, 49152000, codec_mclk_in),
                ACP_CLOCK(bclk, 3072000, codec_slave),
                ACP_CLOCK(fsync, 48000, codec_slave),
                ACP_TDM(2, 32, 3, 3),ACP_SDW_CONFIG_DATA(ACP_SDW, 1, 48000, 2, 0)))
PCM_CAPTURE_ADD(ACP-SW1-PIN1-Capture, 0, PIPELINE_PCM_2)
#/**********************************************************************************/

#/**********************************************************************************/
PIPELINE_PCM_ADD(sof/pipe-passthrough-playback.m4,
	 5, 6, 2, s16le,
 	2000, 0, 0,
        48000, 48000, 48000)

# playback DAI is ACPSP using 2 periods
DAI_ADD(sof/pipe-dai-playback.m4, 5, ACP_SDW, 3, SW0-PIN2-Playback,
PIPELINE_SOURCE_5, 2, s16le, 2000, 0, 0, SCHEDULE_TIME_DOMAIN_DMA)

DAI_CONFIG(ACP_SDW, 1, 3, SW0-PIN2-Playback,
	   ACP_SDW_CONFIG(I2S, ACP_CLOCK(mclk, 49152000, codec_mclk_in),
                ACP_CLOCK(bclk, 3072000, codec_slave),
                ACP_CLOCK(fsync, 48000, codec_slave),
                ACP_TDM(2, 32, 3, 3),ACP_SDW_CONFIG_DATA(ACP_SDW, 1, 48000, 2, 0)))
PCM_PLAYBACK_ADD(ACP-SW0-PIN2-Playback, 6, PIPELINE_PCM_5)
#/**********************************************************************************/
