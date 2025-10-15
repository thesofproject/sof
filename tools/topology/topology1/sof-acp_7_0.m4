#
# Topology for ACP_7_0 with I2SHS and DMIC.
#

# Include topology builder
include(`utils.m4')
include(`dai.m4')
include(`pipeline.m4')
include(`acp-hs.m4')
include(`acp-dmic.m4')

# Include TLV library
include(`common/tlv.m4')

# Include Token library
include(`sof/tokens.m4')

# Include ACP DSP configuration
include(`platform/amd/acp.m4')

DEBUG_START
#======================================================================
# Playback pipeline 1 on PCM 0 using max 2 channels of s16le.

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate)

# Schedule 96 frames per 2000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-passthrough-playback.m4,
        1, 0, 2, s16le,
        2000, 0, 0,
        48000, 48000, 48000)

dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     deadline, priority, core, time_domain)

# playback DAI is ACPHS using 2 periods
DAI_ADD(sof/pipe-dai-playback.m4,
        1, ACPHS, 0, acp-headset-codec,
	PIPELINE_SOURCE_1, 2, s16le,
        2000, 0, 0, SCHEDULE_TIME_DOMAIN_DMA)

dnl DAI_CONFIG(type, dai_index, link_id, name, acphs_config/acpdmic_config)
dnl ACPHS_CONFIG(format, mclk, bclk, fsync, tdm, acphs_config_data)
dnl ACP_CLOCK(clock, freq, codec_provider, polarity)
dnl ACPHS_CONFIG_DATA(type, idx, valid bits, mclk_id)
dnl mclk_id is optional

DAI_CONFIG(ACPHS, 0, 0, acp-headset-codec,
	   ACPHS_CONFIG(I2S, ACP_CLOCK(mclk, 49152000, codec_mclk_in),
                ACP_CLOCK(bclk, 3072000, codec_consumer),
                ACP_CLOCK(fsync, 48000, codec_consumer),
                ACP_TDM(2, 32, 3, 3),ACPHS_CONFIG_DATA(ACPHS, 0, 48000, 2, 0)))

#===========================================================================
# Capture pipeline 2 on PCM 0 using max 2 channels of s16le.
PIPELINE_PCM_ADD(sof/pipe-passthrough-capture.m4,
        2, 0, 2, s16le,
        2000, 0, 0,
        48000, 48000, 48000)

# Capture DAI is ACPHS using 2 periods
DAI_ADD(sof/pipe-dai-capture.m4,
        2, ACPHS, 0, acp-headset-codec,
        PIPELINE_SINK_2, 2, s16le,
        2000, 0, 0, SCHEDULE_TIME_DOMAIN_DMA)

#===========================================================================
# PCM  id 0
PCM_DUPLEX_ADD(I2SHS, 0, PIPELINE_PCM_1, PIPELINE_PCM_2)

#==========================================================================
# Capture pipeline 3 on PCM 1 using max 2 channels of s32le.
PIPELINE_PCM_ADD(sof/pipe-passthrough-capture.m4,
        4, 2, 2, s32le,
        2000, 0, 0,
        48000, 48000, 48000)

DAI_ADD(sof/pipe-dai-capture.m4,
        4, ACPDMIC, 0, acp-dmic-codec,
        PIPELINE_SINK_4, 2, s32le,
        2000, 0, 0, SCHEDULE_TIME_DOMAIN_DMA)

#dnl DAI_CONFIG(type, dai_index, link_id, name, acpdmic_config)
DAI_CONFIG(ACPDMIC, 3, 2, acp-dmic-codec,
	   ACPDMIC_CONFIG(ACPDMIC_CONFIG_DATA(ACPDMIC, 3, 48000, 2)))

#==========================================================================
# PCM id 2
PCM_CAPTURE_ADD(DMIC, 2, PIPELINE_PCM_4)

#==========================================================================

DEBUG_END
