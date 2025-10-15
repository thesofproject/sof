#
# Topology for MT8186 board with mt6366/rt1019/rt5682s/da7219/max98357
#

# Include topology builder
include(`utils.m4')
include(`dai.m4')
include(`pipeline.m4')
include(`afe.m4')
include(`pcm.m4')
include(`buffer.m4')

# Include TLV library
include(`common/tlv.m4')

# Include Token library
include(`sof/tokens.m4')

# Include DSP configuration
include(`platform/mediatek/mt8186.m4')

#
# Define the pipelines
#
# PCM25 ---> AFE (Speaker - max98357/rt1019)
# PCM26 ---> AFE (Headset playback - da7219/rt5682s)
# PCM27 <--- AFE (DMIC - MT6366)
# PCM28 <--- AFE (Headset record - da7219/rt5682s)


dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate,
dnl     time_domain, sched_comp)

define(`ENDPOINT_NAME', `Speakers')
# Low Latency playback pipeline 1 on PCM 25 using max 2 channels of s16le
# Set 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(ifdef(`DTS',sof/pipe-eq-iir-dts-codec-playback.m4, ifdef(`WAVES', sof/pipe-waves-codec-playback.m4, sof/pipe-passthrough-playback.m4)),
	1, 25, 2, s16le,
	1000, 0, 0,
	48000, 48000, 48000)
undefine(`ENDPOINT_NAME')

define(`ENDPOINT_NAME', `Headphones')
# Low Latency playback pipeline 2 on PCM 26 using max 2 channels of s16le
# Set 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(ifdef(`DTS',sof/pipe-eq-iir-dts-codec-playback.m4, ifdef(`WAVES', sof/pipe-waves-codec-playback.m4, sof/pipe-passthrough-playback.m4)),
	2, 26, 2, s16le,
	1000, 0, 0,
	48000, 48000, 48000)
undefine(`ENDPOINT_NAME')

# Low Latency capture pipeline 3 on PCM 27 using max 2 channels of s16le
# Set 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-passthrough-capture.m4,
	3, 27, 2, s16le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency capture pipeline 4 on PCM 28 using max 2 channels of s16le
# Set 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-passthrough-capture.m4,
	4, 28, 2, s16le,
	1000, 0, 0,
	48000, 48000, 48000)

#
# DAIs configuration
#

dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     deadline, priority, core)


# playback DAI is AFE using 2 periods
# Buffers use s16le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	1, AFE, 0, AFE_SOF_DL1,
	PIPELINE_SOURCE_1, 2, s16le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is AFE using 2 periods
# Buffers use s16le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	2, AFE, 1, AFE_SOF_DL2,
	PIPELINE_SOURCE_2, 2, s16le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# capture DAI is AFE using 2 periods
# Buffers use s16le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	3, AFE, 2, AFE_SOF_UL1,
	PIPELINE_SINK_3, 2, s16le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# capture DAI is AFE using 2 periods
# Buffers use s16le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	4, AFE, 3, AFE_SOF_UL2,
	PIPELINE_SINK_4, 2, s16le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

#SCHEDULE_TIME_DOMAIN_DMA
dnl PCM_PLAYBACK_ADD(name, pcm_id, playback)

# PCM Low Latency, id 0
PCM_PLAYBACK_ADD(SOF_DL1, 25, PIPELINE_PCM_1)
PCM_PLAYBACK_ADD(SOF_DL2, 26, PIPELINE_PCM_2)
PCM_CAPTURE_ADD(SOF_UL1, 27, PIPELINE_PCM_3)
PCM_CAPTURE_ADD(SOF_UL2, 28, PIPELINE_PCM_4)

dnl DAI_CONFIG(type, dai_index, link_id, name, afe_config)

DAI_CONFIG(AFE, 0, 0, AFE_SOF_DL1,
	AFE_CONFIG(AFE_CONFIG_DATA(AFE, 0, 48000, 2, s16le)))

DAI_CONFIG(AFE, 1, 0, AFE_SOF_DL2,
	AFE_CONFIG(AFE_CONFIG_DATA(AFE, 1, 48000, 2, s16le)))

DAI_CONFIG(AFE, 2, 0, AFE_SOF_UL1,
	AFE_CONFIG(AFE_CONFIG_DATA(AFE, 2, 48000, 2, s16le)))

DAI_CONFIG(AFE, 3, 0, AFE_SOF_UL2,
	AFE_CONFIG(AFE_CONFIG_DATA(AFE, 3, 48000, 2, s16le)))
