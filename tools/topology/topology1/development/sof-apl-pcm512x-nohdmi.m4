#
# Topology for generic Apollolake UP^2 with pcm512x codec and no HDMI.
#

# Include topology builder
include(`utils.m4')
include(`dai.m4')
include(`pipeline.m4')
include(`ssp.m4')

# Include TLV library
include(`common/tlv.m4')

# Include Token library
include(`sof/tokens.m4')

# Include Apollolake DSP configuration
include(`platform/intel/bxt.m4')

DEBUG_START

#
# Define the pipelines
#
# PPROC is for playback pipeline processing, i.e volume, eq-volume, eq-iir, eq-fir etc
# In this file, volume and eq-volume are used. If anything else is used, please update
# pipeline comments below for tracking purpose.
#
ifelse(PPROC, `volume', `# PCM0 ----> volume -----> SSP5 (pcm512x)',
    `ifelse(PPROC, `eq-volume',
        `# PCM0 ----> EQ IIR ----> EQ FIR ----> volume ----> SSP5 (pcm512x)', `')')
#
# The pipeline naming notation is pipe-PROCESSING-DIRECTION.m4
define(PIPE_PROC_PLAYBACK, `sof/pipe-`PPROC'-playback.m4')


dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate,
dnl     time_domain, sched_comp)

# Low Latency playback pipeline 1 on PCM 0 using max 2 channels of s32le.
# Set 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(PIPE_PROC_PLAYBACK,
	1, 0, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

#
# DAIs configuration
#

dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     deadline, priority, core, time_domain)

# playback DAI is SSP5 using 2 periods
# Buffers use s24le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	1, SSP, 5, SSP5-Codec,
	PIPELINE_SOURCE_1, 2, s24le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# PCM Low Latency, id 0
dnl PCM_PLAYBACK_ADD(name, pcm_id, playback)
PCM_PLAYBACK_ADD(Port5, 0, PIPELINE_PCM_1)

#
# BE configurations - overrides config in ACPI if present
#

#SSP 5 (ID: 0)
DAI_CONFIG(SSP, 5, 0, SSP5-Codec,
	SSP_CONFIG(I2S, SSP_CLOCK(mclk, 24576000, codec_mclk_in),
		SSP_CLOCK(bclk, 3072000, codec_slave),
		SSP_CLOCK(fsync, 48000, codec_slave),
		SSP_TDM(2, 32, 3, 3),
		SSP_CONFIG_DATA(SSP, 5, 24)))

DEBUG_END
