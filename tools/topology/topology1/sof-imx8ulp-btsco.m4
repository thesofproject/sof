#
# Topology for i.MX8ULP board with bluetooth codec
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
# PCM0 <----> volume <-----> SAI5 (connect to BT)
#

# SAI5 is connected to BT which supports only 1 channel for playback and record
ifdef(`CHANNELS_MIN', `', `define(CHANNELS_MIN, 1)')

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate,
dnl     time_domain, sched_comp)

# Low Latency playback pipeline 1 on PCM 0 using max 1 channels of s16le.
# Set 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	1, 0, 1, s16le,
	1000, 0, 0,
	`FSYNC_RATE', `FSYNC_RATE', `FSYNC_RATE')

# Low Latency capture pipeline 2 on PCM 0 using max 1 channels of s16le.
# Set 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-volume-capture.m4,
	2, 0, 1, s16le,
	1000, 0, 0,
	`FSYNC_RATE', `FSYNC_RATE', `FSYNC_RATE')

#
# DAIs configuration
#

dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     period, priority, core, time_domain)

# playback DAI is SAI5 using 2 periods
# Buffers use s16le format, with 16 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	1, SAI, 5, sai5-bt-sco-pcm-wb,
	PIPELINE_SOURCE_1, 2, s16le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# capture DAI is SAI5 using 2 periods
# Buffers use s16le format, with 16 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	2, SAI, 5, sai5-bt-sco-pcm-wb,
	PIPELINE_SINK_2, 2, s16le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# PCM Low Latency, id 0

dnl PCM_DUPLEX_ADD(name, pcm_id, playback, capture)
PCM_DUPLEX_ADD(Port0, 0, PIPELINE_PCM_1, PIPELINE_PCM_2)

# BCLK frequency is computed using the following formula:
#	Freq(BCLK) = Freq(SYNC) * TDM_SLOTS * TDM_SLOT_WIDTH
#
# For 8ULP this yields the following frequencies (based on
# supported BT HFP configurations):
#
#	1) NBS (Freq(FSYNC) = 8k)
#		Freq(BCLK) = 8k * 16 * 1 = 128000
#
#	2) WBS (Freq(FSYNC) = 16k)
#		Freq(BCLK) = 16k * 16 * 1 = 256000
dnl DAI_CONFIG(type, idx, link_id, name, sai_config)
DAI_CONFIG(SAI, 5, 0, sai5-bt-sco-pcm-wb,
	SAI_CONFIG(DSP_A, SAI_CLOCK(mclk, 12288000, codec_mclk_out),
		SAI_CLOCK(bclk, `eval(FSYNC_RATE * 16)', codec_consumer, inverted),
		SAI_CLOCK(fsync, `FSYNC_RATE', codec_consumer),
		SAI_TDM(1, 16, 1, 1),
		SAI_CONFIG_DATA(SAI, 5, 0)))
