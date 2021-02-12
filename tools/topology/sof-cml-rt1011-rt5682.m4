#
# Topology for Cometlake with rt1011 spk on SSP1
#

# if XPROC is not defined, define with default pipe
# Note: DMIC16KPROC is hard coded in sof-cml-rt5682-kwd.m4
ifdef(`DMICPROC', , `define(DMICPROC, passthrough)')

define(DMIC_16k_PCM_NAME, `DMIC16k')

# Include SOF CML RT5682 Topology
# This includes topology for RT5682, DMIC and 3 HDMI Pass through pipeline
include(`sof-cml-rt5682-kwd.m4')
include(`abi.h')
DEBUG_START

#
# Define the Speaker pipeline
#
# PCM5  ----> volume (pipe 7) ----> SSP1 (speaker - rt1011, BE link 5)
#

# The pipeline naming notation is pipe-PROCESSING-DIRECTION.m4
# PPROC is set by makefile
define(PIPE_PROC_PLAYBACK, `sof/pipe-`PPROC'-playback.m4')

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate,
dnl     time_domain, sched_comp)

# Low Latency playback pipeline 7 on PCM 5 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(PIPE_PROC_PLAYBACK,
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

# playback DAI is SSP1 using 2 periods
# Buffers use s16le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	7, SSP, 1, SSP1-Codec,
	PIPELINE_SOURCE_7, 2, s24le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# PCM Low Latency, id 0
dnl PCM_PLAYBACK_ADD(name, pcm_id, playback)
PCM_PLAYBACK_ADD(Speakers, 5, PIPELINE_PCM_7)

#
# BE configurations for Speakers - overrides config in ACPI if present
#

#SSP 1 (ID: 6)
#Use BCLK delay in SSP_CONFIG_DATA only on supporting version

DAI_CONFIG(SSP, SSP1_INDEX, 6, SSP1_NAME,
        SSP_CONFIG(DSP_A, SSP_CLOCK(mclk, SSP1_MCLK_RATE, codec_mclk_in),
                SSP_CLOCK(bclk, 4800000, codec_slave),
                SSP_CLOCK(fsync, 48000, codec_slave),
                SSP_TDM(4, 25, 3, 15),
                SSP_CONFIG_DATA(SET_SSP1_CONFIG_DATA)))

DEBUG_END
