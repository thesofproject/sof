#
# Topology for ACP_7_x with TDM  (24-bit).
#
# Include topology builder
include(`utils.m4')
include(`dai.m4')
include(`pipeline.m4')
include(`acp-tdm.m4')

# Include TLV library
include(`common/tlv.m4')

# Include Token library
include(`sof/tokens.m4')

# Include ACP DSP configuration
include(`platform/amd/acp.m4')

#
#  Pipeline Graph (24-bit / s24le):
#
#  PLAYBACK/CAPTURE:
#  TDM0, TDM1, TDM2 each expose a duplex PCM.
#  Host side uses s32le; DAI side uses s24le (frame_fmt=1).
#

DEBUG_START
#======================================================================
# Playback/Capture pipelines for three TDM instances (24-bit).

# TDM instance 0

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate)
PIPELINE_PCM_ADD(sof/pipe-passthrough-playback.m4,
        0, 0, 2, s32le,
        2000, 0, 0,
        48000, 48000, 48000)

PIPELINE_PCM_ADD(sof/pipe-passthrough-capture.m4,
        3, 0, 2, s32le,
        2000, 0, 0,
        48000, 48000, 48000)

dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     deadline, priority, core, time_domain)
DAI_ADD(sof/pipe-dai-playback.m4,
        0, ACPTDM, 0, acp-i2s0-codec,
        PIPELINE_SOURCE_0, 2, s24le,
        2000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

DAI_ADD(sof/pipe-dai-capture.m4,
        3, ACPTDM, 0, acp-i2s0-codec,
        PIPELINE_SINK_3, 2, s24le,
        2000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

dnl DAI_CONFIG(type, dai_index, link_id, name, ACPTDM_config/acpdmic_config)
dnl ACPTDM_CONFIG(format, mclk, bclk, fsync, tdm, ACPTDM_config_data)
dnl ACP_CLOCK(clock, freq, codec_provider, polarity)
dnl ACPTDM_CONFIG_DATA(type, idx, rate, tdm_mode, frame_fmt(0 for s16le, 1 for s24le, 2 for s32le))
dnl mclk_id is optional
DAI_CONFIG(ACPTDM, 0, 0, acp-i2s0-codec,
           ACPTDM_CONFIG(I2S, ACP_CLOCK(mclk, 49152000, codec_mclk_in),
                ACP_CLOCK(bclk, 3072000, codec_consumer),
                ACP_CLOCK(fsync, 48000, codec_consumer),
                ACP_TDM(2, 32, 3, 3), ACPTDM_CONFIG_DATA(ACPTDM, 0, 48000, 2, 0, 1)))

dnl PCM_DUPLEX_ADD(name, pcm_id, playback_pipeline, capture_pipeline)
PCM_DUPLEX_ADD(I2STDM0, 0, PIPELINE_PCM_0, PIPELINE_PCM_3)

#============================
# TDM instance 1

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate)
PIPELINE_PCM_ADD(sof/pipe-passthrough-playback.m4,
        1, 1, 2, s32le,
        2000, 0, 0,
        48000, 48000, 48000)

PIPELINE_PCM_ADD(sof/pipe-passthrough-capture.m4,
        4, 1, 2, s32le,
        2000, 0, 0,
        48000, 48000, 48000)

dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     deadline, priority, core, time_domain)
DAI_ADD(sof/pipe-dai-playback.m4,
        1, ACPTDM, 1, acp-i2s1-codec,
        PIPELINE_SOURCE_1, 2, s24le,
        2000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

DAI_ADD(sof/pipe-dai-capture.m4,
        4, ACPTDM, 1, acp-i2s1-codec,
        PIPELINE_SINK_4, 2, s24le,
        2000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

dnl DAI_CONFIG(type, dai_index, link_id, name, ACPTDM_config/acpdmic_config)
dnl ACPTDM_CONFIG(format, mclk, bclk, fsync, tdm, ACPTDM_config_data)
dnl ACP_CLOCK(clock, freq, codec_provider, polarity)
dnl ACPTDM_CONFIG_DATA(type, idx, rate, tdm_mode, frame_fmt(0 for s16le, 1 for s24le, 2 for s32le))
dnl mclk_id is optional
DAI_CONFIG(ACPTDM, 1, 1, acp-i2s1-codec,
           ACPTDM_CONFIG(I2S, ACP_CLOCK(mclk, 49152000, codec_mclk_in),
                ACP_CLOCK(bclk, 3072000, codec_consumer),
                ACP_CLOCK(fsync, 48000, codec_consumer),
                ACP_TDM(2, 32, 3, 3), ACPTDM_CONFIG_DATA(ACPTDM, 1, 48000, 2, 0, 1)))

dnl PCM_DUPLEX_ADD(name, pcm_id, playback_pipeline, capture_pipeline)
PCM_DUPLEX_ADD(I2STDM1, 1, PIPELINE_PCM_1, PIPELINE_PCM_4)

#============================
# TDM instance 2

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate)
PIPELINE_PCM_ADD(sof/pipe-passthrough-playback.m4,
        2, 2, 2, s32le,
        2000, 0, 0,
        48000, 48000, 48000)

PIPELINE_PCM_ADD(sof/pipe-passthrough-capture.m4,
        5, 2, 2, s32le,
        2000, 0, 0,
        48000, 48000, 48000)

dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     deadline, priority, core, time_domain)
DAI_ADD(sof/pipe-dai-playback.m4,
        2, ACPTDM, 2, acp-i2s2-codec,
        PIPELINE_SOURCE_2, 2, s24le,
        2000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

DAI_ADD(sof/pipe-dai-capture.m4,
        5, ACPTDM, 2, acp-i2s2-codec,
        PIPELINE_SINK_5, 2, s24le,
        2000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

dnl DAI_CONFIG(type, dai_index, link_id, name, ACPTDM_config/acpdmic_config)
dnl ACPTDM_CONFIG(format, mclk, bclk, fsync, tdm, ACPTDM_config_data)
dnl ACP_CLOCK(clock, freq, codec_provider, polarity)
dnl ACPTDM_CONFIG_DATA(type, idx, rate, tdm_mode, frame_fmt(0 for s16le, 1 for s24le, 2 for s32le))
dnl mclk_id is optional
DAI_CONFIG(ACPTDM, 2, 2, acp-i2s2-codec,
           ACPTDM_CONFIG(I2S, ACP_CLOCK(mclk, 49152000, codec_mclk_in),
                ACP_CLOCK(bclk, 3072000, codec_consumer),
                ACP_CLOCK(fsync, 48000, codec_consumer),
                ACP_TDM(2, 32, 3, 3), ACPTDM_CONFIG_DATA(ACPTDM, 2, 48000, 2, 0, 1)))

dnl PCM_DUPLEX_ADD(name, pcm_id, playback_pipeline, capture_pipeline)
PCM_DUPLEX_ADD(I2STDM2, 2, PIPELINE_PCM_2, PIPELINE_PCM_5)

DEBUG_END
