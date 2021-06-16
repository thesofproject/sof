#
# Topology for ApolloLake with direct attach digital microphones array for
# keyword detection and triggering use case.
#

# Include topology builder
include(`utils.m4')
include(`dai.m4')
include(`pipeline.m4')

# Include TLV library
include(`common/tlv.m4')

# Include Token library
include(`sof/tokens.m4')

# Include Apollolake DSP configuration
include(`platform/intel/bxt.m4')
include(`platform/intel/dmic.m4')

define(KWD_PIPE_SCH_DEADLINE_US, 20000)

define(DMIC_16k_PCM_NAME, `DMIC16k')

#
# Define the pipelines
#
# PCM 0 <-------+- KPBM 0 <-- B0 <-- DMIC6 (DMIC01)
#               |
# Keyword <-----+

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate,
dnl     time_domain, sched_comp)

# Passthrough capture pipeline 1 on PCM 0 using max 2 channels.
# Schedule 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-kfbm-capture.m4,
	1, 0, 2, s16le,
	KWD_PIPE_SCH_DEADLINE_US, 0, 0,
	16000, 16000, 16000)

#
# DAIs configuration
#

dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     deadline, priority, core, time_domain)


# capture DAI is DMIC 0 using 2 periods
# Buffers use s16le format, with 320 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	1, DMIC, 1, NoCodec-6,
	PIPELINE_SINK_1, 2, s16le,
	KWD_PIPE_SCH_DEADLINE_US,
	0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# keyword detector pipe
dnl PIPELINE_ADD(pipeline,
dnl     pipe id, max channels, format,
dnl     period, priority, core,
dnl     sched_comp, time_domain,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate)
PIPELINE_ADD(sof/pipe-detect.m4,
	2, 2, s16le,
	KWD_PIPE_SCH_DEADLINE_US, 1, 0,
	PIPELINE_SCHED_COMP_1,
	SCHEDULE_TIME_DOMAIN_TIMER,
	16000, 16000, 16000)

# Connect pipelines together
SectionGraph."pipe-sof-apl-keyword-detect" {
        index "0"

        lines [
		# keyword detect
                dapm(PIPELINE_SINK_2, PIPELINE_SOURCE_1)
		dapm(PIPELINE_PCM_1, PIPELINE_DETECT_2)
        ]
}

#
# BE configurations - overrides config in ACPI if present
#

dnl DAI_CONFIG(type, dai_index, link_id, name, ssp_config/dmic_config)
DAI_CONFIG(DMIC, 1, 6, NoCodec-6,
           dnl DMIC_CONFIG(driver_version, clk_min, clk_mac, duty_min, duty_max,
           dnl             sample_rate, fifo word length, unmute time, type,
           dnl             dai_index, pdm controller config)
           DMIC_CONFIG(1, 500000, 4800000, 40, 60, 16000,
                DMIC_WORD_LENGTH(s16le), 400, DMIC, 1,
                PDM_CONFIG(DMIC, 1, STEREO_PDM0)))

