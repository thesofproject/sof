#
# Topology for MT8195 board with mt6359/rt5682/rt1019
#

# Include topology builder
include(`utils.m4')
include(`dai.m4')
include(`pipeline.m4')
include(`afe.m4')
include(`pcm.m4')
include(`buffer.m4')
include(`muxdemux.m4')

# Include TLV library
include(`common/tlv.m4')

# Include Token library
include(`sof/tokens.m4')

# Include DSP configuration
include(`platform/mediatek/mt8195.m4')

#
# Define the demux configure
#
dnl Configure demux
dnl name, pipeline_id, routing_matrix_rows
dnl Diagonal 1's in routing matrix mean that every input channel is
dnl copied to corresponding output channels in all output streams.
dnl I.e. row index is the input channel, 1 means it is copied to
dnl corresponding output channel (column index), 0 means it is discarded.
dnl There's a separate matrix for all outputs.
define(matrix1, `ROUTE_MATRIX(1,
			     `BITS_TO_BYTE(1, 0, 0 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 1, 0 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 1 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,1 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,1 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,1 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,0 ,1 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,0 ,0 ,1)')')

define(matrix2, `ROUTE_MATRIX(3,
			     `BITS_TO_BYTE(1, 0, 0 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 1, 0 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 1 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,1 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,1 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,1 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,0 ,1 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,0 ,0 ,1)')')


ifdef(`GOOGLE_RTC_AUDIO_PROCESSING', `MUXDEMUX_CONFIG(demux_priv_1, 2, LIST_NONEWLINE(`', `matrix1,', `matrix2'))')
ifdef(`GOOGLE_RTC_AUDIO_PROCESSING', `define(`SPK_PERIOD_US', 10000)', `define(`SPK_PERIOD_US', 1000)')
ifdef(`GOOGLE_RTC_AUDIO_PROCESSING', `define(`MIC_PERIOD_US', 10000)', `define(`MIC_PERIOD_US', 2000)')

#
# Define the pipelines
#
# PCM16 ---> AFE (Speaker - rt1019)
# PCM17 ---> AFE (Headset playback - rt5682)
# PCM18 <--- AFE (DMIC - MT6365)
# PCM19 <--- AFE (Headset record - rt5682)


dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate,
dnl     time_domain, sched_comp)

# Low Latency playback pipeline 1 on PCM 16 using max 2 channels of s16le
# Set 10000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(
	ifdef(`DTS', sof/pipe-eq-iir-dts-codec-playback.m4,
	ifdef(`GOOGLE_RTC_AUDIO_PROCESSING', sof/pipe-volume-demux-playback.m4, sof/pipe-passthrough-playback.m4)),
	1, 16, 2, s16le,
	SPK_PERIOD_US, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 2 on PCM 17 using max 2 channels of s16le
# Set 10000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(
	ifdef(`DTS', sof/pipe-eq-iir-dts-codec-playback.m4, sof/pipe-passthrough-playback.m4),
	2, 17, 2, s16le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency capture pipeline 3 on PCM 18 using max 2 channels of s16le
# Set 2000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(
	ifdef(`RTNR',
    ifdef(`GOOGLE_RTC_AUDIO_PROCESSING',
    sof/pipe-google-rtc-audio-processing-rtnr-capture.m4,
    sof/pipe-rtnr-capture.m4),
    sof/pipe-passthrough-capture.m4),
	3, 18, 2, s16le,
	MIC_PERIOD_US, 0, 0,
	48000, 48000, 48000)

# Low Latency capture pipeline 4 on PCM 19 using max 2 channels of s16le
# Set 2000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-passthrough-capture.m4,
	4, 19, 2, s16le,
	2000, 0, 0,
	48000, 48000, 48000)

#
# DAIs configuration
#

dnl if using Google AEC
ifdef(`GOOGLE_RTC_AUDIO_PROCESSING',
`# Connect demux to capture'
`SectionGraph."PIPE_GOOGLE_RTC_AUDIO_PROCESSING_REF_AEC" {'
`        index "0"'
`        lines ['
`                # mux to capture'
`                dapm(N_AEC_REF_BUF, PIPELINE_DEMUX_1)'
`       ]'
`}'
dnl else
, `')

dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     deadline, priority, core)


# playback DAI is AFE using 2 periods
# Buffers use s16le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	1, AFE, 0, AFE_SOF_DL2,
	PIPELINE_SOURCE_1, 2, s16le,
	SPK_PERIOD_US, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is AFE using 2 periods
# Buffers use s16le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	2, AFE, 1, AFE_SOF_DL3,
	PIPELINE_SOURCE_2, 2, s16le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# capture DAI is AFE using 2 periods
# Buffers use s16le format, with 48 frame per 2000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	3, AFE, 2, AFE_SOF_UL4,
	PIPELINE_SINK_3, 2, s16le,
	MIC_PERIOD_US, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# capture DAI is AFE using 2 periods
# Buffers use s16le format, with 48 frame per 2000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	4, AFE, 3, AFE_SOF_UL5,
	PIPELINE_SINK_4, 2, s16le,
	2000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

#SCHEDULE_TIME_DOMAIN_DMA
dnl PCM_PLAYBACK_ADD(name, pcm_id, playback)

# PCM Low Latency, id 0
PCM_PLAYBACK_ADD(SOF_DL2, 16, PIPELINE_PCM_1)
PCM_PLAYBACK_ADD(SOF_DL3, 17, PIPELINE_PCM_2)
PCM_CAPTURE_ADD(SOF_UL4, 18, PIPELINE_PCM_3)
PCM_CAPTURE_ADD(SOF_UL5, 19, PIPELINE_PCM_4)

dnl DAI_CONFIG(type, dai_index, link_id, name, afe_config)

DAI_CONFIG(AFE, 0, 0, AFE_SOF_DL2,
	AFE_CONFIG(AFE_CONFIG_DATA(AFE, 0, 48000, 2, s16le)))

DAI_CONFIG(AFE, 1, 0, AFE_SOF_DL3,
	AFE_CONFIG(AFE_CONFIG_DATA(AFE, 1, 48000, 2, s16le)))

DAI_CONFIG(AFE, 2, 0, AFE_SOF_UL4,
	AFE_CONFIG(AFE_CONFIG_DATA(AFE, 2, 48000, 2, s16le)))

DAI_CONFIG(AFE, 3, 0, AFE_SOF_UL5,
	AFE_CONFIG(AFE_CONFIG_DATA(AFE, 3, 48000, 2, s16le)))
