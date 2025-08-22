#
# Topology for i.MX8 board using Virtual DAI (no physical codec)
#

# Include topology builder
include(`utils.m4')
include(`dai.m4')
include(`pipeline.m4')
include(`virtual-dai.m4')
include(`pcm.m4')
include(`buffer.m4')

# Include TLV library
include(`common/tlv.m4')

# Include Token library
include(`sof/tokens.m4')

# Include DSP configuration
include(`platform/imx/imx8.m4')

#
# Define the pipelines
#
# PCM0 ---> Volume ---> Virtual DAI0 (NoCodec)
#

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate,
dnl     time_domain, sched_comp)

# Low Latency playback pipeline 1 on PCM 0 using max 2 channels of s32le
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	1, 0, 2, s32le,
	1000, 0, 0,
	8000, 96000, 48000)

#
# DAIs configuration
#

dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     deadline, priority, core, time_domain)

DAI_ADD(sof/pipe-dai-playback.m4,
	1, DAI_VIRTUAL, 7, NoCodec-0,
	PIPELINE_SOURCE_1, 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

#
# PCM interface
#

dnl PCM_PLAYBACK_ADD(name, pcm_id, playback)
PCM_PLAYBACK_ADD(Port0, 0, PIPELINE_PCM_1)

#
# DAI Configuration
#

dnl DAI_CONFIG(type, dai_index, link_id, name, config)
DAI_CONFIG(DAI_VIRTUAL, 7, 0, NoCodec-0,
	DAI_VIRTUAL_CONFIG(I2S, DAI_VIRTUAL_CLOCK(mclk, 0, dai_provider),
		DAI_VIRTUAL_CLOCK(bclk, 0, dai_provider),
		DAI_VIRTUAL_CLOCK(fsync, 0, dai_provider),
		DAI_VIRTUAL_TDM(2, 32, 3, 3),
		DAI_VIRTUAL_CONFIG_DATA(DAI_VIRTUAL, 7, 0)))
