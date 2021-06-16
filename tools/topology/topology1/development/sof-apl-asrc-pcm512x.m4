#
# Topology for Apollolake UP^2 with pcm512x codec for testing ASRC
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
include(`platform/intel/dmic.m4')

DEBUG_START

#
# Define the pipelines
#
# PCM0 --> ASRC --> Volume --> SSP5 (pcm512x)
# PCM5 <-- ASRC <-- Volume <-- DMIC0 (DMIC)
# PCM6 <-- ASRC <-- Volume <-- DMIC1 (DMIC16kHz)

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate,
dnl	time_domain, sched_comp)

# Playback pipeline 1 on PCM 0 using max 2 channels of s32le.
# Set 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-asrc-volume-playback.m4,
	1, 0, 2, s32le,
	1000, 0, 0,
	8000, 192000, 48000)

# DMIC capture pipeline 2 on PCM 5 using max 2 channels.
# 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-asrc-volume-capture.m4,
        2, 5, 2, s32le,
        1000, 0, 0,
        8000, 192000, 48000)

# DMIC16kHz capture pipeline 2 on PCM 6 using max 2 channels.
# 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-asrc-volume-capture.m4,
        3, 6, 2, s32le,
        1000, 0, 0,
        8000, 192000, 16000)

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

# capture DAI is DMIC using 2 periods
# Buffers use s32le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
        2, DMIC, 0, dmic01,
        PIPELINE_SINK_2, 2, s32le,
        1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# capture DAI is DMIC16kHz using 2 periods
# Buffers use s16le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
        3, DMIC, 1, dmic16k,
        PIPELINE_SINK_3, 2, s32le,
        1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# PCM, id 0
dnl PCM_PLAYBACK_ADD(name, pcm_id, playback)
PCM_PLAYBACK_ADD(Port5, 0, PIPELINE_PCM_1)
PCM_CAPTURE_ADD(DMIC, 5, PIPELINE_PCM_2)
PCM_CAPTURE_ADD(DMIC16kHz, 6, PIPELINE_PCM_3)

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

# DMIC (ID: 1)
DAI_CONFIG(DMIC, 0, 1, dmic01,
           DMIC_CONFIG(1, 2400000, 4800000, 40, 60, 48000,
                DMIC_WORD_LENGTH(s32le), 400, DMIC, 0,
                PDM_CONFIG(DMIC, 0, STEREO_PDM0)))

# DMIC16kHz (ID: 2)
DAI_CONFIG(DMIC, 1, 2, dmic16k,
           DMIC_CONFIG(1, 2400000, 4800000, 40, 60, 16000,
                DMIC_WORD_LENGTH(s32le), 400, DMIC, 1,
                PDM_CONFIG(DMIC, 1, STEREO_PDM0)))

DEBUG_END
