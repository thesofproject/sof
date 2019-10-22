#
# Topology for Cometlake with rt5682 headset on SSP0, max98357a spk on SSP1
#
# Adding max98357a spk on SSP1 on top of sof-cml-rt5682.
#

# Include SOF CML RT5682 Topology
# This includes topology for RT5682, DMIC and 3 HDMI Pass through pipeline
include(`sof-cml-demux-rt5682.m4')

# Define the Speaker pipeline
#
# PCM6  ----> volume (pipe 8) ----> SSP1 (speaker - maxim98357a, BE link 5)
#

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate,
dnl     time_domain, sched_comp)

# Low Latency playback pipeline 8 on PCM 6 using max 2 channels of s32le.
# Schedule 1000us deadline on core 0 with priority 0
# pipe-src pipeline is needed for older SSP config

ifelse(SOF_ABI_VERSION_3_9_OR_GRT, `1',
`PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
        8, 6, 2, s32le,
        1000, 0, 0,
        48000, 48000, 48000)',
`PIPELINE_PCM_ADD(sof/pipe-src-volume-playback.m4,
        8, 6, 2, s32le,
        1000, 0, 0,
        48000, 48000, 48000)')

#
# Speaker DAIs configuration
#

dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     deadline, priority, core, time_domain)

# playback DAI is SSP1 using 2 periods
# Buffers use s16le format, with 48 frame per 1000us on core 0 with priority 0
#With m/n divider available we can support 24 bit playback

ifelse(SOF_ABI_VERSION_3_9_OR_GRT, `1',
`DAI_ADD(sof/pipe-dai-playback.m4,
        8, SSP, 1, SSP1-Codec,
        PIPELINE_SOURCE_7, 2, s24le,
        1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)',
`DAI_ADD(sof/pipe-dai-playback.m4,
        8, SSP, 1, SSP1-Codec,
        PIPELINE_SOURCE_7, 2, s16le,
        1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)')

# PCM Low Latency, id 0
dnl PCM_PLAYBACK_ADD(name, pcm_id, playback)
PCM_PLAYBACK_ADD(Speakers, 6, PIPELINE_PCM_8)

#
# BE configurations for Speakers - overrides config in ACPI if present
#

#SSP 1 (ID: 6)
# Use BCLK derived using m/n divider only on later versions

ifelse(SOF_ABI_VERSION_3_9_OR_GRT, `1',
`DAI_CONFIG(SSP, 1, 6, SSP1-Codec,
        SSP_CONFIG(I2S, SSP_CLOCK(mclk, 24000000, codec_mclk_in),
                SSP_CLOCK(bclk, 2304000, codec_slave),
                SSP_CLOCK(fsync, 48000, codec_slave),
                SSP_TDM(2, 24, 3, 3),
                SSP_CONFIG_DATA(SSP, 1, 24)))',
`DAI_CONFIG(SSP, 1, 6, SSP1-Codec,
        SSP_CONFIG(I2S, SSP_CLOCK(mclk, 24000000, codec_mclk_in),
                SSP_CLOCK(bclk, 1500000, codec_slave),
                SSP_CLOCK(fsync, 46875, codec_slave),
                SSP_TDM(2, 16, 3, 3),
                SSP_CONFIG_DATA(SSP, 1, 16)))')
