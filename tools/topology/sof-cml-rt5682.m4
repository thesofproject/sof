#
# Topology for Cometlake with rt5682 codec.
#

# Include topology builder
include(`utils.m4')
include(`dai.m4')
include(`pipeline.m4')
include(`ssp.m4')

# Include TLV library
include(`common/tlv.m4')

# Include Token library
include(`sof/tokens.m4')

# Include Cannonlake or Icelake DSP configuration
ifelse(PLATFORM, `icl', include(`platform/intel/icl.m4'),
	ifelse(PLATFORM, `whl', include(`platform/intel/cnl.m4'),
		ifelse(PLATFORM, `cml', include(`platform/intel/cnl.m4'), `')))
include(`platform/intel/dmic.m4')

DEBUG_START

undefine(`SSP_INDEX')
define(`SSP_INDEX', ifelse(PLATFORM, `icl', `0',
	ifelse(PLATFORM, `whl', `1',
		ifelse(PLATFORM, `cml', `0', `'))))

undefine(`SSP_NAME')
define(`SSP_NAME', ifelse(PLATFORM, `icl', `SSP0-Codec',
	ifelse(PLATFORM, `whl', `SSP1-Codec',
		ifelse(PLATFORM, `cml', `SSP0-Codec', `'))))

undefine(`SSP_MCLK_RATE')
define(`SSP_MCLK_RATE', ifelse(PLATFORM, `icl', `19200000',
	ifelse(PLATFORM, `whl', `24000000',
		ifelse(PLATFORM, `cml', `24000000', `'))))

#
# Define the pipelines
#
# PCM0 <---> volume <----> SSP(SSP_INDEX, BE link 0)
# PCM1 <------------------ DMIC01 (dmic0 capture, , BE link 1)
# PCM2 ----> volume -----> iDisp1 (HDMI/DP playback, BE link 3)
# PCM3 ----> volume -----> iDisp2 (HDMI/DP playback, BE link 4)
# PCM4 ----> volume -----> iDisp3 (HDMI/DP playback, BE link 5)
# PCM5 <------------------ DMIC16k (dmic16k, BE link 2)
#

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     frames, deadline, priority, core)

# Low Latency playback pipeline 1 on PCM 0 using max 2 channels of s24le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	1, 0, 2, s24le,
	48, 1000, 0, 0)

# Low Latency capture pipeline 2 on PCM 0 using max 2 channels of s24le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-capture.m4,
	2, 0, 2, s24le,
	48, 1000, 0, 0)

# Passthrough capture pipeline 3 on PCM 1 using max 4 channels.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-passthrough-capture.m4,
	3, 1, 4, s32le,
	48, 1000, 0, 0)

# Low Latency playback pipeline 4 on PCM 2 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	4, 2, 2, s32le,
	48, 1000, 0, 0)

# Low Latency playback pipeline 5 on PCM 3 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	5, 3, 2, s32le,
	48, 1000, 0, 0)

# Low Latency playback pipeline 6 on PCM 4 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	6, 4, 2, s32le,
	48, 1000, 0, 0)

# Passthrough capture pipeline 7 on PCM 5 using max 2 channels.
# Schedule 16 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-passthrough-capture.m4,
	7, 5, 2, s16le,
	16, 1000, 0, 0)

#
# DAIs configuration
#

dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     frames, deadline, priority, core)

# playback DAI is SSP(SPP_INDEX) using 2 periods
# Buffers use s24le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	1, SSP, SSP_INDEX, SSP_NAME,
	PIPELINE_SOURCE_1, 2, s24le,
	48, 1000, 0, 0)

# capture DAI is SSP(SSP_INDEX) using 2 periods
# Buffers use s24le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	2, SSP,SSP_INDEX, SSP_NAME,
	PIPELINE_SINK_2, 2, s24le,
	48, 1000, 0, 0)

# capture DAI is DMIC01 using 2 periods
# Buffers use s32le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	3, DMIC, 0, dmic01,
	PIPELINE_SINK_3, 2, s32le,
	48, 1000, 0, 0)

# playback DAI is iDisp1 using 2 periods
# Buffers use s32le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	4, HDA, 0, iDisp1,
	PIPELINE_SOURCE_4, 2, s32le,
	48, 1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is iDisp2 using 2 periods
# Buffers use s32le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	5, HDA, 1, iDisp2,
	PIPELINE_SOURCE_5, 2, s32le,
	48, 1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is iDisp3 using 2 periods
# Buffers use s32le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	6, HDA, 2, iDisp3,
	PIPELINE_SOURCE_6, 2, s32le,
	48, 1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# capture DAI is DMIC16k using 2 periods
# Buffers use s16le format, with 16 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	7, DMIC, 1, dmic16k,
	PIPELINE_SINK_7, 2, s16le,
	16, 1000, 0, 0)


# PCM Low Latency, id 0
dnl PCM_PLAYBACK_ADD(name, pcm_id, playback)
PCM_DUPLEX_ADD(Port1, 0, PIPELINE_PCM_1, PIPELINE_PCM_2)
PCM_CAPTURE_ADD(DMIC01, 1, PIPELINE_PCM_3)
PCM_PLAYBACK_ADD(HDMI1, 2, PIPELINE_PCM_4)
PCM_PLAYBACK_ADD(HDMI2, 3, PIPELINE_PCM_5)
PCM_PLAYBACK_ADD(HDMI3, 4, PIPELINE_PCM_6)
PCM_CAPTURE_ADD(DMIC16k, 5, PIPELINE_PCM_7)

#
# BE configurations - overrides config in ACPI if present
#

#SSP SSP_INDEX (ID: 0)
DAI_CONFIG(SSP, SSP_INDEX, 0, SSP_NAME,
	SSP_CONFIG(I2S, SSP_CLOCK(mclk, SSP_MCLK_RATE, codec_mclk_in),
		      SSP_CLOCK(bclk, 2400000, codec_slave),
		      SSP_CLOCK(fsync, 48000, codec_slave),
		      SSP_TDM(2, 25, 3, 3),
		      SSP_CONFIG_DATA(SSP, SSP_INDEX, 24)))

# dmic01 (ID: 1)
DAI_CONFIG(DMIC, 0, 1, dmic01,
	   DMIC_CONFIG(1, 500000, 4800000, 40, 60, 48000,
		DMIC_WORD_LENGTH(s32le), 400, DMIC, 0,
		PDM_CONFIG(DMIC, 0, FOUR_CH_PDM0_PDM1)))

# dmic16k (ID: 2)
DAI_CONFIG(DMIC, 1, 2, dmic16k,
	   DMIC_CONFIG(1, 500000, 4800000, 40, 60, 16000,
		DMIC_WORD_LENGTH(s16le), 400, DMIC, 1,
		PDM_CONFIG(DMIC, 1, STEREO_PDM0)))

# 3 HDMI/DP outputs (ID: 3,4,5)
DAI_CONFIG(HDA, 0, 3, iDisp1)
DAI_CONFIG(HDA, 1, 4, iDisp2)
DAI_CONFIG(HDA, 2, 5, iDisp3)

DEBUG_END
