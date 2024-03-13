#
# Topology for i.MX8QXP/i.MX8QM/i.MX8MP boards with `CODEC' codec
#
# CODEC: wm8960, wm8962
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
# PCM0 <----> SRC <----> volume <-----> `SAI_INDEX' (`CODEC')
#

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate,
dnl     time_domain, sched_comp)

# Low Latency playback pipeline 1 on PCM 0 using max 2 channels of s32le.
# Set 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-`PPROC'-volume-playback.m4,
	1, 0, 2, s32le,
	1000, 0, 0,
	8000, 96000, `RATE')

# Low Latency capture pipeline 2 on PCM 0 using max 2 channels of s32le.
# Set 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-`PPROC'-volume-capture.m4,
	2, 0, 2, s32le,
	1000, 0, 0,
	8000, 96000, `RATE')
#
# DAIs configuration
#

# define STREAM_NAME, based on CODEC name
define(`STREAM_NAME',
	`ifelse(CODEC, `wm8960', `-wm8960-hifi',
			CODEC, `wm8962', `-wm8962',
			`fatal_error(`Codec not supported.')')')

# define DAI BE dai_link name
define(`DAI_BE_NAME', concat(concat(`sai', SAI_INDEX), STREAM_NAME))

dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     period, priority, core, time_domain)

# playback DAI is SAI_SAI_INDEX using 2 periods
# Buffers use s32le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	1, SAI, SAI_INDEX, DAI_BE_NAME,
	PIPELINE_SOURCE_1, 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# capture DAI is SAI_SAI_INDEX using 2 periods
# Buffers use s32le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	2, SAI, SAI_INDEX, DAI_BE_NAME,
	PIPELINE_SINK_2, 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)


# PCM Low Latency, id 0

dnl PCM_DUPLEX_ADD(name, pcm_id, playback, capture)
PCM_DUPLEX_ADD(Port0, 0, PIPELINE_PCM_1, PIPELINE_PCM_2)

dnl DAI_CONFIG(type, idx, link_id, name, sai_config)
DAI_CONFIG(SAI, SAI_INDEX, 0, DAI_BE_NAME,
ifelse(
	CODEC, `wm8960', `
	SAI_CONFIG(I2S, SAI_CLOCK(mclk, 12288000, codec_mclk_in),
		SAI_CLOCK(bclk, 3072000, codec_provider),
		SAI_CLOCK(fsync, RATE, codec_provider),
		SAI_TDM(2, 32, 3, 3),
		SAI_CONFIG_DATA(SAI, SAI_INDEX, 0)))',
	CODEC, `wm8962', `
	SAI_CONFIG(I2S, SAI_CLOCK(mclk, 12288000, codec_mclk_in),
		SAI_CLOCK(bclk, 3072000, codec_provider),
		SAI_CLOCK(fsync, `RATE', codec_provider),
		SAI_TDM(2, 32, 3, 3),
		SAI_CONFIG_DATA(SAI, SAI_INDEX, 0)))',
	)
