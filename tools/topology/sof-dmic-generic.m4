#
# Topology for SKL+ HDA Generic machine w/ iDISP codec only
#

# Include topology builder
include(`utils.m4')
include(`dai.m4')
include(`pipeline.m4')

# Include TLV library
include(`common/tlv.m4')

# Include Token library
include(`sof/tokens.m4')

# Include bxt DSP configuration
include(`platform/intel/bxt.m4')

include(`platform/intel/dmic.m4')

#
# Define the pipelines
#

dnl PIPELINE_PCM_DAI_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     dai type, dai_index, dai format,
dnl     dai periods, pcm_min_rate, pcm_max_rate,
dnl     pipeline_rate, time_domain)

# Passthrough capture pipeline using max channels defined by CHANNELS.

# Set 1000us deadline on core 0 with priority 0
PIPELINE_PCM_DAI_ADD(sof/pipe-eq-capture.m4,
	5, 0, CHANNELS, s32le,
	1000, 0, 0, DMIC, 0, s32le, 3, 48000, 48000, 48000)

# Passthrough capture pipeline using max channels defined by CHANNELS.

# Schedule with 1000us deadline on core 0 with priority 0
PIPELINE_PCM_DAI_ADD(sof/pipe-eq-capture-16khz.m4,
	6, 1, CHANNELS, s32le,
	1000, 0, 0, DMIC, 1, s32le, 3, 16000, 16000, 16000)

#
# DAIs configuration
#

dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     deadline, priority, core, time_domain)

# capture DAI is DMIC 0 using 3 periods
# Buffers use s32le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	5, DMIC, 0, DMIC-0,
	PIPELINE_SINK_5, 3, s32le,
	1000, 0, 0, 48000, 48000, 48000)

# capture DAI is DMIC 1 using 3 periods
# Buffers use s32le format, with 16 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	6, DMIC, 1, DMIC-1,
	PIPELINE_SINK_6, 3, s32le,
	1000, 0, 0, 16000, 16000, 16000)

dnl PCM_DUPLEX_ADD(name, pcm_id, playback, capture)
dnl PCM_CAPTURE_ADD(name, pipeline, capture)
PCM_CAPTURE_ADD(DMIC48kHz, 0, PIPELINE_PCM_5)
PCM_CAPTURE_ADD(DMIC16kHz, 1, PIPELINE_PCM_6)

#
# BE configurations - overrides config in ACPI if present
#

dnl DAI_CONFIG(type, dai_index, link_id, name, ssp_config/dmic_config)
ifelse(CHANNELS, 4,
`DAI_CONFIG(DMIC, 0, 0, DMIC-0,
	   DMIC_CONFIG(1, 500000, 4800000, 40, 60, 48000,
		DMIC_WORD_LENGTH(s32le), 200, DMIC, 0,
		PDM_CONFIG(DMIC, 0, FOUR_CH_PDM0_PDM1)))',
`DAI_CONFIG(DMIC, 0, 0, DMIC-0,
           DMIC_CONFIG(1, 500000, 4800000, 40, 60, 48000,
                DMIC_WORD_LENGTH(s32le), 200, DMIC, 0,
                PDM_CONFIG(DMIC, 0, STEREO_PDM0)))')

ifelse(CHANNELS, 4,
`DAI_CONFIG(DMIC, 1, 1, DMIC-1,
	   DMIC_CONFIG(1, 500000, 4800000, 40, 60, 16000,
		DMIC_WORD_LENGTH(s32le), 400, DMIC, 1,
		PDM_CONFIG(DMIC, 1, FOUR_CH_PDM0_PDM1)))',
`DAI_CONFIG(DMIC, 1, 1, DMIC-1,
           DMIC_CONFIG(1, 500000, 4800000, 40, 60, 16000,
                DMIC_WORD_LENGTH(s32le), 400, DMIC, 1,
                PDM_CONFIG(DMIC, 1, STEREO_PDM0)))')
