#
# Topology for Vangogh with I2S SP and I2S HS.
#

# Include topology builder
include(`utils.m4')
include(`dai.m4')
include(`pipeline.m4')
include(`acp-hs.m4')
include(`acp-sp.m4')

# Include TLV library
include(`common/tlv.m4')

# Include Token library
include(`sof/tokens.m4')

# Include ACP DSP configuration
include(`platform/amd/acp.m4')

# Playback pipeline 1 on PCM 0 using max 2 channels of s16le.
# Schedule 96 frames per 2000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-passthrough-playback.m4,
        1, 0, 2, s16le,
        2000, 0, 0,
        48000, 48000, 48000)

# playback DAI is ACPSP using 2 periods
DAI_ADD(sof/pipe-dai-playback.m4, 1, ACPSP, 0, acp-headset-codec,
PIPELINE_SOURCE_1, 2, s16le, 2000, 0, 0, SCHEDULE_TIME_DOMAIN_DMA)

DAI_CONFIG(ACPSP, 0, 0, acp-headset-codec,
	   ACPSP_CONFIG(I2S, ACP_CLOCK(mclk, 49152000, codec_mclk_in),
                ACP_CLOCK(bclk, 3072000, codec_consumer),
                ACP_CLOCK(fsync, 48000, codec_consumer),
                ACP_TDM(2, 32, 3, 3),ACPSP_CONFIG_DATA(ACPSP, 0, 48000, 2, 0)))

PIPELINE_PCM_ADD(sof/pipe-passthrough-playback.m4,
	 2, 1, 2, s16le,
 	2000, 0, 0,
        48000, 48000, 48000)

# playback DAI is ACPSP using 2 periods
DAI_ADD(sof/pipe-dai-playback.m4, 2, ACPHS, 1, acp-amp-codec,
PIPELINE_SOURCE_2, 2, s16le, 2000, 0, 0, SCHEDULE_TIME_DOMAIN_DMA)

DAI_CONFIG(ACPHS, 1, 1, acp-amp-codec,
	   ACPHS_CONFIG(I2S, ACP_CLOCK(mclk, 49152000, codec_mclk_in),
                ACP_CLOCK(bclk, 3072000, codec_consumer),
                ACP_CLOCK(fsync, 48000, codec_consumer),
                ACP_TDM(2, 32, 3, 3),ACPHS_CONFIG_DATA(ACPHS, 1, 48000, 2, 0)))


# Capture pipeline 2 on PCM 0 using max 2 channels of s16le.
PIPELINE_PCM_ADD(sof/pipe-passthrough-capture.m4,
        3, 0, 2, s16le,
        2000, 0, 0,
        48000, 48000, 48000)

# Capture DAI is ACPSP using 2 periods
DAI_ADD(sof/pipe-dai-capture.m4, 3, ACPSP, 0, acp-headset-codec,
PIPELINE_SINK_3, 2, s16le, 2000, 0, 0, SCHEDULE_TIME_DOMAIN_DMA)

# PCM  id 0
PCM_DUPLEX_ADD(I2SSP, 0, PIPELINE_PCM_1, PIPELINE_PCM_3)

# PCM  id 1
PCM_PLAYBACK_ADD(I2SHS, 1, PIPELINE_PCM_2)
