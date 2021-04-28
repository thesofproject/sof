#
# Topology for i.MX8 with wm8960 codec for
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

include(`sai.m4')
# Include i.MX8 DSP configuration
include(`platform/imx/imx8.m4')

define(KWD_PIPE_SCH_DEADLINE_US, 20000)

define(DMIC_16k_PCM_NAME, `SAI3')

# This is for iMX8M PLUS
#
# PCM 0 <-------+- KPBM 0 <-- B0 <-- SAI3 (wm8960)
#               |
# Keyword <-----+
dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate,
dnl     time_domain, sched_comp)

# Passthrough capture pipeline 1 on PCM 0 using max 2 channels.
# Schedule 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-kfbm-no-lp-capture.m4,
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

# This is for iMX8M PLUS
# capture DAI is SAI3 using 2 periods
# Buffers use s16le format, with 320 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
        1, SAI, 3, sai3-wm8960-hifi,
        PIPELINE_SINK_1, 2, s16le,
        KWD_PIPE_SCH_DEADLINE_US,
        0, 0, SCHEDULE_TIME_DOMAIN_DMA)

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

# This is for iMX8M PLUS
#
# BE configurations - overrides config in ACPI if present
#

dnl DAI_CONFIG(type, dai_index, link_id, name, sai_config/ssp_config/dmic_config)
DAI_CONFIG(SAI, 3, 0, sai3-wm8960-hifi,
           SAI_CONFIG(I2S, SAI_CLOCK(mclk, 12288000, codec_mclk_in),
                SAI_CLOCK(bclk, 3072000, codec_master),
                SAI_CLOCK(fsync, 16000, codec_master),
                SAI_TDM(2, 32, 3, 3),
                SAI_CONFIG_DATA(SAI, 3, 0)))
