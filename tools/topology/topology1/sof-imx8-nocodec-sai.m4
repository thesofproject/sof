#
# Topology for i.MX8QXP board with nocodec
#

# Include topology builder
include(`utils.m4')
include(`dai.m4')
include(`pipeline.m4')
include(`sai.m4')
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
# PCM0 ---> Volume ---> SAI1 (NoCodec)
#

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate,
dnl     time_domain, sched_comp)

# Low Latency playback pipeline 1 on PCM 0 using max 2 channels of s24le
# Set 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	1, 0, 2, s24le,
	1000, 0, 0,
	8000, 96000, 48000)

#
# DAIs configuration
#

dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     deadline, priority, core)

# playback DAI is SAI1 using 2 periods
# Buffers use s24le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	1, SAI, 1, NoCodec-0,
	PIPELINE_SOURCE_1, 2, s24le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_DMA)

dnl PCM_PLAYBACK_ADD(name, pcm_id, playback)

# PCM Low Latency, id 0
PCM_PLAYBACK_ADD(Port0, 0, PIPELINE_PCM_1)

dnl DAI_CONFIG(type, dai_index, link_id, name, sai_config)
DAI_CONFIG(SAI, 1, 0, NoCodec-0,
	SAI_CONFIG(I2S, SAI_CLOCK(mclk, 49152000, codec_mclk_in),
		SAI_CLOCK(bclk, 3072000, codec_slave),
		SAI_CLOCK(fsync, 48000, codec_slave),
		SAI_TDM(2, 32, 3, 3),
		SAI_CONFIG_DATA(SAI, 1, 0)))
