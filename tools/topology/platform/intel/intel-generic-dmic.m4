#
# Topology for digital microphones array
#

include(`platform/intel/dmic.m4')

# defined in machine driver
define(DMIC_DAI_LINK_48k_ID, `6')
define(DMIC_DAI_LINK_16k_ID, `7')

#
# Define the pipelines
#

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate,
dnl     time_domain, sched_comp)

# Passthrough capture pipeline using max channels defined by CHANNELS.

# Set 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-eq-capture.m4,
	DMIC_PIPELINE_48k_ID, DMIC_DAI_LINK_48k_ID, CHANNELS, s32le,
	1000, 0, 0, 48000, 48000, 48000)

# Passthrough capture pipeline using max channels defined by CHANNELS.

# Schedule with 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-eq-capture-16khz.m4,
	DMIC_PIPELINE_16k_ID, DMIC_DAI_LINK_16k_ID, CHANNELS, s32le,
	1000, 0, 0, 16000, 16000, 16000)

#
# DAIs configuration
#

dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     deadline, priority, core, time_domain)

# capture DAI is DMIC 0 using 2 periods
# Buffers use s32le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	DMIC_PIPELINE_48k_ID, DMIC, 0, dmic01,
	concat(`PIPELINE_SINK_', DMIC_PIPELINE_48k_ID), 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# capture DAI is DMIC 1 using 2 periods
# Buffers use s32le format, with 16 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	DMIC_PIPELINE_16k_ID, DMIC, 1, dmic16k,
	concat(`PIPELINE_SINK_', DMIC_PIPELINE_16k_ID), 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

dnl PCM_DUPLEX_ADD(name, pcm_id, playback, capture)
dnl PCM_CAPTURE_ADD(name, pipeline, capture)
PCM_CAPTURE_ADD(DMIC, DMIC_DAI_LINK_48k_ID, concat(`PIPELINE_PCM_', DMIC_PIPELINE_48k_ID))
PCM_CAPTURE_ADD(DMIC16kHz, DMIC_DAI_LINK_16k_ID, concat(`PIPELINE_PCM_', DMIC_PIPELINE_16k_ID))

#
# BE configurations - overrides config in ACPI if present
#

dnl DAI_CONFIG(type, dai_index, link_id, name, ssp_config/dmic_config)
ifelse(CHANNELS, 4,
`DAI_CONFIG(DMIC, 0, DMIC_DAI_LINK_48k_ID, dmic01,
	   DMIC_CONFIG(1, 500000, 4800000, 40, 60, 48000,
		DMIC_WORD_LENGTH(s32le), 200, DMIC, 0,
		PDM_CONFIG(DMIC, 0, FOUR_CH_PDM0_PDM1)))',
`DAI_CONFIG(DMIC, 0, DMIC_DAI_LINK_48k_ID, dmic01,
           DMIC_CONFIG(1, 500000, 4800000, 40, 60, 48000,
                DMIC_WORD_LENGTH(s32le), 200, DMIC, 0,
                PDM_CONFIG(DMIC, 0, STEREO_PDM0)))')

ifelse(CHANNELS, 4,
`DAI_CONFIG(DMIC, 1, DMIC_DAI_LINK_16k_ID, dmic16k,
	   DMIC_CONFIG(1, 500000, 4800000, 40, 60, 16000,
		DMIC_WORD_LENGTH(s32le), 400, DMIC, 1,
		PDM_CONFIG(DMIC, 1, FOUR_CH_PDM0_PDM1)))',
`DAI_CONFIG(DMIC, 1, DMIC_DAI_LINK_16k_ID, dmic16k,
           DMIC_CONFIG(1, 500000, 4800000, 40, 60, 16000,
                DMIC_WORD_LENGTH(s32le), 400, DMIC, 1,
                PDM_CONFIG(DMIC, 1, STEREO_PDM0)))')
