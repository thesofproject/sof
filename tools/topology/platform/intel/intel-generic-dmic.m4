#
# Topology for digital microphones array
#

include(`platform/intel/dmic.m4')

# defined in machine driver
ifdef(`DMIC_DAI_LINK_48k_ID',`',
`define(DMIC_DAI_LINK_48k_ID, `6')')
ifdef(`DMIC_DAI_LINK_16k_ID',`',
`define(DMIC_DAI_LINK_16k_ID, `7')')

# define(DMIC_DAI_LINK_48k_NAME, `dmic01')
ifdef(`DMIC_DAI_LINK_48k_NAME',`',define(DMIC_DAI_LINK_48k_NAME, `dmic01'))

# define(DMIC_DAI_LINK_16k_NAME, `dmic16k')
ifdef(`DMIC_DAI_LINK_16k_NAME',`',define(DMIC_DAI_LINK_16k_NAME, `dmic16k'))

# Handle possible different channels count for PCM and DAI
ifdef(`DMIC_DAI_CHANNELS', `', `define(DMIC_DAI_CHANNELS, CHANNELS)')
ifdef(`DMIC_PCM_CHANNELS', `', `define(DMIC_PCM_CHANNELS, CHANNELS)')
ifdef(`DMIC16K_DAI_CHANNELS', `', `define(DMIC16K_DAI_CHANNELS, CHANNELS)')
ifdef(`DMIC16K_PCM_CHANNELS', `', `define(DMIC16K_PCM_CHANNELS, CHANNELS)')

ifelse(CHANNELS, 1, `define(DMIC_VOL_CH_MAPS, LIST(`	', KCONTROL_CHANNEL(FL, 1, 0)))')
ifelse(CHANNELS, 2, `define(DMIC_VOL_CH_MAPS, LIST(`	', KCONTROL_CHANNEL(FL, 1, 0),
							KCONTROL_CHANNEL(FR, 1, 1)))')
ifelse(CHANNELS, 3, `define(DMIC_VOL_CH_MAPS, LIST(`	', KCONTROL_CHANNEL(FL, 1, 0),
							KCONTROL_CHANNEL(FC, 1, 1),
							KCONTROL_CHANNEL(FR, 1, 2)))')
ifelse(CHANNELS, 4, `define(DMIC_VOL_CH_MAPS, LIST(`	', KCONTROL_CHANNEL(FLW, 1, 0),
							KCONTROL_CHANNEL(FL, 1, 1),
							KCONTROL_CHANNEL(FR, 1, 2),
							KCONTROL_CHANNEL(FRW, 1, 3)))')

ifelse(CHANNELS, 1, `define(DMIC_SW_CH_MAPS, LIST(`	', KCONTROL_CHANNEL(FL, 2, 0)))')
ifelse(CHANNELS, 2, `define(DMIC_SW_CH_MAPS, LIST(`	', KCONTROL_CHANNEL(FL, 2, 0),
							KCONTROL_CHANNEL(FR, 2, 1)))')
ifelse(CHANNELS, 3, `define(DMIC_SW_CH_MAPS, LIST(`	', KCONTROL_CHANNEL(FL, 2, 0),
							KCONTROL_CHANNEL(FC, 2, 1),
							KCONTROL_CHANNEL(FR, 2, 2)))')
ifelse(CHANNELS, 4, `define(DMIC_SW_CH_MAPS, LIST(`	', KCONTROL_CHANNEL(FLW, 2, 0),
							KCONTROL_CHANNEL(FL, 2, 1),
							KCONTROL_CHANNEL(FR, 2, 2),
							KCONTROL_CHANNEL(FRW, 2, 3)))')
#
# Define the pipelines
#

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate,
dnl     time_domain, sched_comp)

# Passthrough capture pipeline using max channels defined by DMIC_PCM_CHANNELS.

# Set 1000us deadline on core 0 with priority 0
ifdef(`DMICPROC_FILTER1', `define(PIPELINE_FILTER1, DMICPROC_FILTER1)')
ifdef(`DMICPROC_FILTER2', `define(PIPELINE_FILTER2, DMICPROC_FILTER2)')

PIPELINE_PCM_ADD(sof/pipe-DMICPROC-capture.m4,
	DMIC_PIPELINE_48k_ID, DMIC_DAI_LINK_48k_ID, DMIC_PCM_CHANNELS, s32le,
	1000, 0, 0, 48000, 48000, 48000)

undefine(`PIPELINE_FILTER1')
undefine(`PIPELINE_FILTER2')

# Passthrough capture pipeline using max channels defined by CHANNELS.

# Schedule with 1000us deadline on core 0 with priority 0
ifdef(`DMIC16KPROC_FILTER1', `define(PIPELINE_FILTER1, DMIC16KPROC_FILTER1)')
ifdef(`DMIC16KPROC_FILTER2', `define(PIPELINE_FILTER2, DMIC16KPROC_FILTER2)')

PIPELINE_PCM_ADD(sof/pipe-DMIC16KPROC-capture-16khz.m4,
	DMIC_PIPELINE_16k_ID, DMIC_DAI_LINK_16k_ID, DMIC16K_PCM_CHANNELS, s32le,
	1000, 0, 0, 16000, 16000, 16000)

undefine(`PIPELINE_FILTER1')
undefine(`PIPELINE_FILTER2')

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
	DMIC_PIPELINE_48k_ID, DMIC, 0, DMIC_DAI_LINK_48k_NAME,
	concat(`PIPELINE_SINK_', DMIC_PIPELINE_48k_ID), 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# capture DAI is DMIC 1 using 2 periods
# Buffers use s32le format, with 16 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	DMIC_PIPELINE_16k_ID, DMIC, 1, DMIC_DAI_LINK_16k_NAME,
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
ifelse(DMIC_DAI_CHANNELS, 4,
`DAI_CONFIG(DMIC, 0, DMIC_DAI_LINK_48k_ID, DMIC_DAI_LINK_48k_NAME,
	   DMIC_CONFIG(1, 2400000, 4800000, 40, 60, 48000,
		DMIC_WORD_LENGTH(s32le), 200, DMIC, 0,
		PDM_CONFIG(DMIC, 0, FOUR_CH_PDM0_PDM1)))',
`DAI_CONFIG(DMIC, 0, DMIC_DAI_LINK_48k_ID, DMIC_DAI_LINK_48k_NAME,
           DMIC_CONFIG(1, 2400000, 4800000, 40, 60, 48000,
                DMIC_WORD_LENGTH(s32le), 200, DMIC, 0,
                PDM_CONFIG(DMIC, 0, STEREO_PDM0)))')

ifelse(DMIC16K_DAI_CHANNELS, 4,
`DAI_CONFIG(DMIC, 1, DMIC_DAI_LINK_16k_ID, DMIC_DAI_LINK_16k_NAME,
	   DMIC_CONFIG(1, 2400000, 4800000, 40, 60, 16000,
		DMIC_WORD_LENGTH(s32le), 400, DMIC, 1,
		PDM_CONFIG(DMIC, 1, FOUR_CH_PDM0_PDM1)))',
`DAI_CONFIG(DMIC, 1, DMIC_DAI_LINK_16k_ID, DMIC_DAI_LINK_16k_NAME,
           DMIC_CONFIG(1, 2400000, 4800000, 40, 60, 16000,
                DMIC_WORD_LENGTH(s32le), 400, DMIC, 1,
                PDM_CONFIG(DMIC, 1, STEREO_PDM0)))')
