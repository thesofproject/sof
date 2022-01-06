#
# Topology for Cometlake with rt5682 headset on SSP0, max98357a spk on SSP1
#
# Adding max98357a spk on SSP1 on top of sof-cml-rt5682.
#

# Include SOF CML RT5682 Topology
# This includes topology for RT5682, DMIC and 3 HDMI Pass through pipeline
include(`sof-cml-rt5682-kwd.m4')
include(`abi.h')

DEBUG_START
#
# Define the Speaker pipeline
#
# PCM5  ----> volume (pipe 7) ----> SSP1 (speaker - maxim98357a, BE link 5)
#

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate,
dnl     time_domain, sched_comp)

PIPELINE_PCM_ADD(PIPE_VOLUME_PLAYBACK,
	7, 5, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

#
# Speaker DAIs configuration
#

dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     deadline, priority, core, time_domain)


DAI_ADD(sof/pipe-dai-playback.m4,
        7, SSP, SSP1_INDEX, SSP1_NAME,
        PIPELINE_SOURCE_7, 2, SSP1_VALID_BITS_STR,
        1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# PCM Low Latency, id 0
dnl PCM_PLAYBACK_ADD(name, pcm_id, playback)
PCM_PLAYBACK_ADD(Speakers, 5, PIPELINE_PCM_7)

#
# BE configurations for Speakers - overrides config in ACPI if present
#

#SSP 1 (ID: 6)
# Use BCLK derived using m/n divider only on later versions

DAI_CONFIG(SSP, SSP1_INDEX, 6, SSP1_NAME,
        SSP_CONFIG(I2S, SSP_CLOCK(mclk, SSP1_MCLK_RATE, codec_mclk_in),
                SSP_CLOCK(bclk, SSP1_BCLK, codec_consumer),
                SSP_CLOCK(fsync, SSP1_FSYNC, codec_consumer),
                SSP_TDM(2, SSP1_VALID_BITS, 3, 3),
                SSP_CONFIG_DATA(SSP, SSP1_INDEX, SSP1_VALID_BITS)))

DEBUG_END
