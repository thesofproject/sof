#
# Topology for i.MX8QXP/i.MX8QM with wm8960 (cpu board) + cs42888 (base board)
#

# Include topology builder
include(`utils.m4')
include(`dai.m4')
include(`pipeline.m4')
include(`esai.m4')
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
# PCM0 <----> volume <-----> ESAI0 (cs42888)
# PCM1 <----> volume <-----> SAI1 (wm8960)

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate,
dnl     time_domain, sched_comp)

# Low Latency playback pipeline 1 on PCM 0 using max 2 channels of s24le.
# Set 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	1, 0, 2, s24le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency capture pipeline 2 on PCM 0 using max 2 channels of s24le.
# Set 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-volume-capture.m4,
	2, 0, 2, s24le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 1 on PCM 1 using max 2 channels of s32le.
# Set 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	3, 1, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency capture pipeline 2 on PCM 1 using max 2 channels of s32le.
# Set 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-volume-capture.m4,
	4, 1, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)
#
# DAIs configuration
#

dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     period, priority, core, time_domain)

# playback DAI is ESAI0 using 2 periods
# Buffers use s24le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	1, ESAI, 0, esai0-cs42888,
	PIPELINE_SOURCE_1, 2, s24le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# capture DAI is ESAI0 using 2 periods
# Buffers use s24le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	2, ESAI, 0, esai0-cs42888,
	PIPELINE_SINK_2, 2, s24le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is SAI1 using 2 periods
# Buffers use s32le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	3, SAI, 1, sai1-wm8960-hifi,
	PIPELINE_SOURCE_3, 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# capture DAI is SAI1 using 2 periods
# Buffers use s32le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	4, SAI, 1, sai1-wm8960-hifi,
	PIPELINE_SINK_4, 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)


dnl PCM_DUPLEX_ADD(name, pcm_id, playback, capture)
PCM_DUPLEX_ADD(Port0, 0, PIPELINE_PCM_1, PIPELINE_PCM_2)
PCM_DUPLEX_ADD(Port1, 1, PIPELINE_PCM_3, PIPELINE_PCM_4)

dnl DAI_CONFIG(type, idx, link_id, name, esai_config)
DAI_CONFIG(ESAI, 0, 0, esai0-cs42888,
	ESAI_CONFIG(I2S, ESAI_CLOCK(mclk, 49152000, codec_mclk_in),
		ESAI_CLOCK(bclk, 3072000, codec_consumer),
		ESAI_CLOCK(fsync, 48000, codec_consumer),
		ESAI_TDM(2, 32, 3, 3),
		ESAI_CONFIG_DATA(ESAI, 0, 0)))

dnl DAI_CONFIG(type, idx, link_id, name, sai_config)
DAI_CONFIG(SAI, 1, 0, sai1-wm8960-hifi,
	SAI_CONFIG(I2S, SAI_CLOCK(mclk, 12288000, codec_mclk_in),
		SAI_CLOCK(bclk, 3072000, codec_provider),
		SAI_CLOCK(fsync, 48000, codec_provider),
		SAI_TDM(2, 32, 3, 3),
		SAI_CONFIG_DATA(SAI, 1, 0)))
