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

define(DMIC_16k_PCM_NAME, `SAI1')

# This is for iMX8QM/QXP
#
# PCM 0 <-------+- KPBM 0 <-- B0 <-- SAI1 (wm8960)
#               |
# Keyword <-----+
dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate,
dnl     time_domain, sched_comp)

# Passthrough capture pipeline 1 on PCM 0 using max 2 channels.
# Schedule 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-kfbm-no-lp-capture.m4,
	1, 0, 2, s32le,
	KWD_PIPE_SCH_DEADLINE_US, 0, 0,
	16000, 16000, 16000)

#
# DAIs configuration
#

dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     deadline, priority, core, time_domain)

# This is for iMX8QM/QXP
# capture DAI is SAI1 using 2 periods
# Buffers use s32le format, with 320 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	1, SAI, 1, sai1-wm8960-hifi,
	PIPELINE_SINK_1, 2, s32le,
	KWD_PIPE_SCH_DEADLINE_US,
	0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# keyword detector pipe
dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate,
dnl     time_domain, sched_comp, dynamic)
PIPELINE_PCM_ADD(sof/pipe-detect.m4,
	2, 0, 2, s32le,
	KWD_PIPE_SCH_DEADLINE_US, 1, 0,
	16000, 16000, 16000,
	SCHEDULE_TIME_DOMAIN_TIMER,
	PIPELINE_SCHED_COMP_1)

# Connect pipelines together
SectionGraph."pipe-sof-imx8-keyword-detect" {
	index "0"
	lines [
		# keyword detect
		dapm(PIPELINE_SINK_2, PIPELINE_SOURCE_1)
		dapm(PIPELINE_PCM_1, PIPELINE_DETECT_2)
	]
}

dnl DAI_CONFIG(type, dai_index, link_id, name, sai_config)
DAI_CONFIG(SAI, 1, 0, sai1-wm8960-hifi,
	SAI_CONFIG(I2S, SAI_CLOCK(mclk, 12288000, codec_mclk_in),
	SAI_CLOCK(bclk, 3072000, codec_provider),
	SAI_CLOCK(fsync, 16000, codec_provider),
	SAI_TDM(2, 32, 3, 3),
	SAI_CONFIG_DATA(SAI, 1, 0)))
