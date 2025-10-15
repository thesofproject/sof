#
# Topology for imx8mp using SAI2 and SAI3, 8 channels each, playback and capture
#

#
# necessary device tree configuration:
# simple-audio-card with dai-link@0 (cpu <&dsp 1>) and dai-link@1 (cpu <&dsp 2>)
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

define(`CHANNELS', 8)
define(`DEADLINE', 1000)

#
# Define the pipelines
#
# PCM0 <---> Volume <--->  SAI2
# PCM1 <---> Volume <--->  SAI3

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate,
dnl     time_domain, sched_comp)

# Low Latency playback pipeline 1 on PCM 0 using max CHANNELS, s16le
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	1, 0, CHANNELS, s16le,
	DEADLINE, 0, 0,
	48000, 48000, 48000)


# Low Latency capture pipeline 2 on PCM 0 using max CHANNELS, s16le
PIPELINE_PCM_ADD(sof/pipe-volume-capture.m4,
	2, 0, CHANNELS, s16le,
	DEADLINE, 0, 0,
	48000, 48000, 48000)


# Low Latency playback pipeline 3 on PCM 1 using max CHANNELS, s16le
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	3, 1, CHANNELS, s16le,
	DEADLINE, 0, 0,
	48000, 48000, 48000)


# Low Latency capture pipeline 4 on PCM 1 using max CHANNELS, s16le
PIPELINE_PCM_ADD(sof/pipe-volume-capture.m4,
	4, 1, CHANNELS, s16le,
	DEADLINE, 0, 0,
	48000, 48000, 48000)

#
# DAI configuration
#

dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     deadline, priority, core)

# playback DAI SAI2 using 2 periods
DAI_ADD(sof/pipe-dai-playback.m4,
	1, SAI, 2, sai2-bt-sco-pcm-wb,
	PIPELINE_SOURCE_1, 2, s16le,
	DEADLINE, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# capture DAI SAI2 using 2 periods
DAI_ADD(sof/pipe-dai-capture.m4,
	2, SAI, 2, sai2-bt-sco-pcm-wb,
	PIPELINE_SINK_2, 2, s16le,
	DEADLINE, 0, 0)

# playback DAI SAI3 using 2 periods
DAI_ADD(sof/pipe-dai-playback.m4,
	3, SAI, 3, sai3-bt-sco-pcm-wb,
	PIPELINE_SOURCE_3, 2, s16le,
	DEADLINE, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# capture DAI SAI3 using 2 periods
DAI_ADD(sof/pipe-dai-capture.m4,
	4, SAI, 3, sai3-bt-sco-pcm-wb,
	PIPELINE_SINK_4, 2, s16le,
	DEADLINE, 0, 0)

dnl PCM_DUPLEX_ADD(name, pcm_id, playback, capture)
PCM_DUPLEX_ADD(Port0, 0, PIPELINE_PCM_1, PIPELINE_PCM_2)
PCM_DUPLEX_ADD(Port1, 1, PIPELINE_PCM_3, PIPELINE_PCM_4)

dnl DAI_CONFIG(type, dai_index, link_id, name, sai_config)
dnl SAI_CLOCK: clock, freq, codec_provider, polarity (optional)
dnl SAI_TDM: number of channels, word width, tx mask, rx mask: decimal channel enable bits
dnl SAI_CONFIG_DATA: index, mclk_id (optional)

DAI_CONFIG(SAI, 2, 0, sai2-bt-sco-pcm-wb,
	SAI_CONFIG(DSP_A, SAI_CLOCK(mclk, 12288000, codec_mclk_out),
		SAI_CLOCK(bclk, 6144000, codec_consumer),
		SAI_CLOCK(fsync, 48000, codec_consumer),
		SAI_TDM(CHANNELS, 16, 255, 255),
		SAI_CONFIG_DATA(SAI, 2, 0)))

DAI_CONFIG(SAI, 3, 0, sai3-bt-sco-pcm-wb,
	SAI_CONFIG(DSP_A, SAI_CLOCK(mclk, 12288000, codec_mclk_out),
		SAI_CLOCK(bclk, 6144000, codec_consumer, inverted),
		SAI_CLOCK(fsync, 48000, codec_consumer),
		SAI_TDM(CHANNELS, 16, 255, 255),
		SAI_CONFIG_DATA(SAI, 3, 0)))

