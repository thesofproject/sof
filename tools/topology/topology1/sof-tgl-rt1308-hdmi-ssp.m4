#
# Topology for Tigerlake with rt1308 codec + DMIC + 3 HDMI out + 2 HDMI-in capture
#

# Include topology builder
include(`utils.m4')
include(`dai.m4')
include(`pipeline.m4')
include(`ssp.m4')
include(`hda.m4')

# Include TLV library
include(`common/tlv.m4')

# Include Token library
include(`sof/tokens.m4')

# Include Tigerlake DSP configuration
include(`platform/intel/'PLATFORM`.m4')
include(`platform/intel/dmic.m4')

define(`HDMI1_SSP_NAME', concat(concat(`SSP', HDMI_1_SSP_NUM),`-HDMI'))
define(`HDMI2_SSP_NAME', concat(concat(`SSP', HDMI_2_SSP_NUM),`-HDMI'))
define(`AMP_SSP_NAME', concat(concat(`SSP', AMP_SSP_NUM),`-Codec'))

ifdef(`NO_AMP',`',`
ifdef(`AMP_SSP_NUM',`',`fatal_error(note: Define AMP_SSP_NUM for speaker amp SSP)')')

DEBUG_START
#
# Define the pipelines
#
# PCM0 <---- volume <----- HDMI-1 SSP
# PCM1 <---- volume <----- HDMI-2 SSP
# PCM2 ----> volume -----> Codec SSP
# PCM3 <---- volume <----- DMIC01 (dmic0 capture)
#

# PCM5  ----> volume (pipe 5)   -----> iDisp1 (HDMI/DP playback, BE link 5)
# PCM6  ----> Volume (pipe 6)   -----> iDisp2 (HDMI/DP playback, BE link 6)
# PCM7  ----> volume (pipe 7)   -----> iDisp3 (HDMI/DP playback, BE link 7)


dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     frames, deadline, priority, core)

# Low Latency capture pipeline 1 on PCM 0 using max 2 channels of s16le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-capture.m4,
	1, 0, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)


# Low Latency capture pipeline 2 on PCM 1 using max 2 channels of s16le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-capture.m4,
	2, 1, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

ifdef(`NO_AMP',`',`
# Low Latency playback pipeline 3 on PCM 2 using max 2 channels of s24le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	3, 2, 2, s24le,
	1000, 0, 0,
	48000, 48000, 48000)')

# Passthrough capture pipeline 4 on PCM 3 using max 4 channels.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-passthrough-capture.m4,
	4, 3, 4, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 5 on PCM 5 using max 2 channels of s24le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	5, 5, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 6 on PCM 6 using max 2 channels of s24le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	6, 6, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 7 on PCM 7 using max 2 channels of s24le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	7, 7, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

#
# DAIs configuration
#

dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     frames, deadline, priority, core)

# capture DAI using 2 periods
# Buffers use s16le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	1, SSP, HDMI_1_SSP_NUM, HDMI1_SSP_NAME,
	PIPELINE_SINK_1, 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# capture DAI using 2 periods
# Buffers use s16le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	2, SSP, HDMI_2_SSP_NUM, HDMI2_SSP_NAME,
	PIPELINE_SINK_2, 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

ifdef(`NO_AMP',`',`
# playback DAI using 2 periods
# Buffers use s24le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	3, SSP, AMP_SSP_NUM, AMP_SSP_NAME,
	PIPELINE_SOURCE_3, 2, s24le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)')

# capture DAI is DMIC01 using 2 periods
# Buffers use s32le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	4, DMIC, 0, dmic01,
	PIPELINE_SINK_4, 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is iDisp1 using 2 periods
# Buffers use s32le format, 1000us deadline with priority 0 on core 0
DAI_ADD(sof/pipe-dai-playback.m4,
        5, HDA, 5, iDisp1,
        PIPELINE_SOURCE_5, 2, s32le,
        1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is iDisp2 using 2 periods
# Buffers use s32le format, 1000us deadline with priority 0 on core 0
DAI_ADD(sof/pipe-dai-playback.m4,
        6, HDA, 6, iDisp2,
        PIPELINE_SOURCE_6, 2, s32le,
        1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is iDisp3 using 2 periods
# Buffers use s32le format, 1000us deadline with priority 0 on core 0
DAI_ADD(sof/pipe-dai-playback.m4,
        7, HDA, 7, iDisp3,
        PIPELINE_SOURCE_7, 2, s32le,
        1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# PCM Low Latency, id 0
dnl PCM_CAPTURE_ADD(name, pcm_id, capture)
PCM_CAPTURE_ADD(HDMI-IN-1, 0, PIPELINE_PCM_1)

dnl PCM_CAPTURE_ADD(name, pcm_id, capture)
PCM_CAPTURE_ADD(HDMI-IN-2, 1, PIPELINE_PCM_2)

ifdef(`NO_AMP',`',`
dnl PCM_PLAYBACK_ADD(name, pcm_id, playback)
PCM_PLAYBACK_ADD(Speaker, 2, PIPELINE_PCM_3)')
PCM_CAPTURE_ADD(DMIC01, 3, PIPELINE_PCM_4)

PCM_PLAYBACK_ADD(HDMI 1, 5, PIPELINE_PCM_5)
PCM_PLAYBACK_ADD(HDMI 2, 6, PIPELINE_PCM_6)
PCM_PLAYBACK_ADD(HDMI 3, 7, PIPELINE_PCM_7)

#
# BE configurations - overrides config in ACPI if present
#

#HDMI-1 SSP (ID: 0)
#MCLK is not required and won't impact for HDMI-in capture use case.
DAI_CONFIG(SSP, HDMI_1_SSP_NUM, 0, HDMI1_SSP_NAME,
	   SSP_CONFIG(I2S, SSP_CLOCK(mclk, 38400000, codec_mclk_in),
		      SSP_CLOCK(bclk, 3072000, codec_master),
		      SSP_CLOCK(fsync, 48000, codec_master),
		      SSP_TDM(2, 32, 3, 3),
		      SSP_CONFIG_DATA(SSP, HDMI_1_SSP_NUM, 32, 0)))

#HDMI-2 SSP (ID: 1)
#MCLK is not required and won't impact for HDMI-in capture use case.
DAI_CONFIG(SSP, HDMI_2_SSP_NUM, 1, HDMI2_SSP_NAME,
	   SSP_CONFIG(I2S, SSP_CLOCK(mclk, 38400000, codec_mclk_in),
		      SSP_CLOCK(bclk, 3072000, codec_master),
		      SSP_CLOCK(fsync, 48000, codec_master),
		      SSP_TDM(2, 32, 3, 3),
		      SSP_CONFIG_DATA(SSP, HDMI_2_SSP_NUM, 32, 0)))

ifdef(`NO_AMP',`',`
#Amplifier Codec SSP (ID: 2)
DAI_CONFIG(SSP, AMP_SSP_NUM, 2, AMP_SSP_NAME,
	SSP_CONFIG(I2S, SSP_CLOCK(mclk, 38400000, codec_mclk_in),
		      SSP_CLOCK(bclk, 2400000, codec_slave),
		      SSP_CLOCK(fsync, 48000, codec_slave),
		      SSP_TDM(2, 25, 3, 3),
		      SSP_CONFIG_DATA(SSP, AMP_SSP_NUM, 24)))')

# dmic01 (ID: 3)
DAI_CONFIG(DMIC, 0, 3, dmic01,
	   DMIC_CONFIG(1, 500000, 4800000, 40, 60, 48000,
		DMIC_WORD_LENGTH(s32le), 400, DMIC, 0,
		PDM_CONFIG(DMIC, 0, FOUR_CH_PDM0_PDM1)))


# 3 HDMI/DP outputs (ID: 5,6,7)
DAI_CONFIG(HDA, 5, 5, iDisp1,
	HDA_CONFIG(HDA_CONFIG_DATA(HDA, 5, 48000, 2)))
DAI_CONFIG(HDA, 6, 6, iDisp2,
	HDA_CONFIG(HDA_CONFIG_DATA(HDA, 6, 48000, 2)))
DAI_CONFIG(HDA, 7, 7, iDisp3,
	HDA_CONFIG(HDA_CONFIG_DATA(HDA, 7, 48000, 2)))
DEBUG_END
