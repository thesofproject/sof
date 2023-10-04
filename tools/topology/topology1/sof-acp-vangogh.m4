#
# Topology for Vangogh with I2S SP, I2S HS and I2S BT.
#

# Include topology builder
include(`utils.m4')
include(`dai.m4')
include(`pipeline.m4')
include(`acp-hs.m4')
include(`acp-sp.m4')
include(`acp-bt.m4')

# Include TLV library
include(`common/tlv.m4')

# Include Token library
include(`sof/tokens.m4')

# Include ACP DSP configuration
include(`platform/amd/acp.m4')

DEBUG_START

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
                ACP_CLOCK(bclk, 3072000, codec_slave),
                ACP_CLOCK(fsync, 48000, codec_slave),
                ACP_TDM(2, 32, 3, 3),ACPSP_CONFIG_DATA(ACPSP, 0, 48000, 2, 0)))



# Capture pipeline 2 on PCM 0 using max 2 channels of s16le.
PIPELINE_PCM_ADD(sof/pipe-passthrough-capture.m4,
        3, 0, 2, s16le,
        2000, 0, 0,
        48000, 48000, 48000)

# Capture DAI is ACPSP using 2 periods
DAI_ADD(sof/pipe-dai-capture.m4, 3, ACPSP, 0, acp-headset-codec,
PIPELINE_SINK_3, 2, s16le, 2000, 0, 0, SCHEDULE_TIME_DOMAIN_DMA)



# Playback pipeline 4 on PCM 2 using max 2 channels of s16le.
# Schedule 96 frames per 2000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-passthrough-playback.m4,
        4, 2, 2, s16le,
        2000, 0, 0,
        48000, 48000, 48000)

# playback DAI is ACPBT using 2 periods
DAI_ADD(sof/pipe-dai-playback.m4, 4, ACPBT, 2, acp-bt-codec,
PIPELINE_SOURCE_4, 2, s16le, 2000, 0, 0, SCHEDULE_TIME_DOMAIN_DMA)

DAI_CONFIG(ACPBT, 2, 2, acp-bt-codec,
	   ACPBT_CONFIG(I2S, ACP_CLOCK(mclk, 49152000, codec_mclk_in),
                ACP_CLOCK(bclk, 3072000, codec_slave),
                ACP_CLOCK(fsync, 48000, codec_slave),
                ACP_TDM(2, 32, 3, 3),ACPBT_CONFIG_DATA(ACPBT, 2, 48000, 2, 0)))

# Capture pipeline 5 on PCM 2 using max 2 channels of s16le.
PIPELINE_PCM_ADD(sof/pipe-passthrough-capture.m4,
        5, 2, 2, s16le,
        2000, 0, 0,
        48000, 48000, 48000)

# Capture DAI is ACPBT using 2 periods
DAI_ADD(sof/pipe-dai-capture.m4, 5, ACPBT, 2, acp-bt-codec,
PIPELINE_SINK_5, 2, s16le, 2000, 0, 0, SCHEDULE_TIME_DOMAIN_DMA)

# PCM  id 0
PCM_DUPLEX_ADD(I2SSP, 0, PIPELINE_PCM_1, PIPELINE_PCM_3)

# PCM  id 1
include(`sof-smart-amplifier-amd-nocodec.m4')

# PCM  id 2
PCM_DUPLEX_ADD(I2SBT, 2, PIPELINE_PCM_4, PIPELINE_PCM_5)

DEBUG_END

