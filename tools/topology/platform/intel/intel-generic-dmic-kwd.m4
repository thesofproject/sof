#
# Topology for digital microphones array
#

include(`platform/intel/dmic.m4')

# variable that need to be defined in upper m4
# define(CHANNELS, `4') define channel for the dmic
ifdef(`CHANNELS',`',`errprint(note: Need to define channel number for intel-generic-dmic-kwd
)')
# define(KFBM_TYPE, `vol-kfbm') define kfbm type, available type: vol-kfbm/kfbm
ifdef(`KFBM_TYPE',`',`errprint(note: Need to define kfbm type for intel-generic-dmic-kwd, available type: vol-kfbm/kfbm
)')
# define(DMIC_PCM_48k_ID, `99')
ifdef(`DMIC_PCM_48k_ID',`',`errprint(note: Need to define dmic48k pcm id for intel-generic-dmic-kwd
)')
# define(DMIC_PIPELINE_48k_ID, `4')
ifdef(`DMIC_PIPELINE_48k_ID',`',`errprint(note: Need to define dmic48k pipeline id for intel-generic-dmic-kwd
)')
# define(DMIC_DAI_LINK_48k_ID, `1')
ifdef(`DMIC_DAI_LINK_48k_ID',`',`errprint(note: Need to define dmic48k dai id for intel-generic-dmic-kwd
)')

# define(DMIC_PCM_16k_ID, `7')
ifdef(`DMIC_PCM_16k_ID',`',`errprint(note: Need to define dmic16k pcm id for intel-generic-dmic-kwd
)')
# define(DMIC_PIPELINE_16k_ID, `9')
ifdef(`DMIC_PIPELINE_16k_ID',`',`errprint(note: Need to define dmic16k pipeline id for intel-generic-dmic-kwd
)')
# define(DMIC_PIPELINE_KWD_ID, `10')
ifdef(`DMIC_PIPELINE_KWD_ID',`',`errprint(note: Need to define kwd pipeline id for intel-generic-dmic-kwd
)')
# define(DMIC_DAI_LINK_16k_ID, `2')
ifdef(`DMIC_DAI_LINK_16k_ID',`',`errprint(note: Need to define dmic16k dai id for intel-generic-dmic-kwd
)')

# define(KWD_PIPE_SCH_DEADLINE_US, 20000)
ifdef(`KWD_PIPE_SCH_DEADLINE_US',`',`errprint(note: Need to define schedule for intel-generic-dmic-kwd
)')

# define(DMIC_DAI_LINK_48k_NAME, `dmic01')
ifdef(`DMIC_DAI_LINK_48k_NAME',`',define(DMIC_DAI_LINK_48k_NAME, `dmic01'))

# define(DMIC_DAI_LINK_16k_NAME, `dmic16k')
ifdef(`DMIC_DAI_LINK_16k_NAME',`',define(DMIC_DAI_LINK_16k_NAME, `dmic16k'))

# DMICPROC is set by makefile, available type: passthrough/eq-iir-volume
ifdef(`DMICPROC', , `define(DMICPROC, passthrough)')

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
PIPELINE_PCM_ADD(sof/pipe-`DMICPROC'-capture.m4,
        DMIC_PIPELINE_48k_ID, DMIC_PCM_48k_ID, CHANNELS, s32le,
        1000, 0, 0, 48000, 48000, 48000)

#
# KWD configuration
#

# Passthrough capture pipeline 7 on PCM 7 using max 2 channels.
# Schedule 20000us deadline on core 0 with priority 0
PIPELINE_PCM_DAI_ADD(sof/pipe-KFBM_TYPE-capture.m4,
        DMIC_PIPELINE_16k_ID, DMIC_PCM_16k_ID, 2, s32le,
        KWD_PIPE_SCH_DEADLINE_US, 0, 0, DMIC, 1, s32le, 3,
        16000, 16000, 16000)



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

# capture DAI is DMIC 1 using 3 periods
# Buffers use s32le format, with 320 frame per 20000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
        DMIC_PIPELINE_16k_ID, DMIC, 1, DMIC_DAI_LINK_16k_NAME,
        `PIPELINE_SINK_'DMIC_PIPELINE_16k_ID, 3, s32le,
        KWD_PIPE_SCH_DEADLINE_US, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

dnl PCM_DUPLEX_ADD(name, pcm_id, playback, capture)
dnl PCM_CAPTURE_ADD(name, pipeline, capture)
PCM_CAPTURE_ADD(DMIC48kHz, DMIC_PCM_48k_ID, concat(`PIPELINE_PCM_', DMIC_PIPELINE_48k_ID))

# keyword detector pipe
dnl PIPELINE_ADD(pipeline,
dnl     pipe id, max channels, format,
dnl     period, priority, core,
dnl     sched_comp, time_domain,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate)
PIPELINE_ADD(sof/pipe-detect.m4,
        DMIC_PIPELINE_KWD_ID, 2, s24le,
        KWD_PIPE_SCH_DEADLINE_US, 1, 0,
        `PIPELINE_SCHED_COMP_'DMIC_PIPELINE_16k_ID,
        SCHEDULE_TIME_DOMAIN_TIMER,
        16000, 16000, 16000)

# Connect pipelines together
SectionGraph."pipe-sof-generic-keyword-detect" {
        index "0"

        lines [
                # keyword detect
                dapm(`PIPELINE_SINK_'DMIC_PIPELINE_KWD_ID, `PIPELINE_SOURCE_'DMIC_PIPELINE_16k_ID)
                dapm(`PIPELINE_PCM_'DMIC_PIPELINE_16k_ID, `PIPELINE_DETECT_'DMIC_PIPELINE_KWD_ID)
        ]
}

#
# BE configurations - overrides config in ACPI if present
#

dnl DAI_CONFIG(type, dai_index, link_id, name, ssp_config/dmic_config)
ifelse(CHANNELS, 4,
`DAI_CONFIG(DMIC, 0, DMIC_DAI_LINK_48k_ID, DMIC_DAI_LINK_48k_NAME,
                DMIC_CONFIG(1, 2400000, 4800000, 40, 60, 48000,
                DMIC_WORD_LENGTH(s32le), 200, DMIC, 0,
                PDM_CONFIG(DMIC, 0, FOUR_CH_PDM0_PDM1)))',
`DAI_CONFIG(DMIC, 0, DMIC_DAI_LINK_48k_ID, DMIC_DAI_LINK_48k_NAME,
                DMIC_CONFIG(1, 2400000, 4800000, 40, 60, 48000,
                DMIC_WORD_LENGTH(s32le), 200, DMIC, 0,
                PDM_CONFIG(DMIC, 0, STEREO_PDM0)))')

# dmic16k (ID: 2)
DAI_CONFIG(DMIC, 1, DMIC_DAI_LINK_16k_ID, DMIC_DAI_LINK_16k_NAME,
                DMIC_CONFIG(1, 2400000, 4800000, 40, 60, 16000,
                DMIC_WORD_LENGTH(s32le), 400, DMIC, 1,
                PDM_CONFIG(DMIC, 1, STEREO_PDM0)))
