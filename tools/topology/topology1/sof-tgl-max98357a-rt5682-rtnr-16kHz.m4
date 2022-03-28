#
# Topology for Tigerlake with CODEC amp + rt5682 codec + DMIC + 4 HDMI
#

# Include topology builder
include(`utils.m4')
include(`dai.m4')
include(`pipeline.m4')
include(`ssp.m4')
include(`muxdemux.m4')
include(`hda.m4')

# Include TLV library
include(`common/tlv.m4')

# Include Token library
include(`sof/tokens.m4')

# Include Tigerlake DSP configuration
include(`platform/intel/tgl.m4')
include(`platform/intel/dmic.m4')
DEBUG_START


#
# Define the pipelines
#
# PCM0 <---> volume <----> SSP0     (Headset - ALC5682)
# PCM1 <---- volume <----- DMIC01   (dmic 48k capture)
# PCM2 <---- RTNR   <----- DMIC16K  (dmic 16k capture)
# PCM3 ----> volume -----> iDisp1
# PCM4 ----> volume -----> iDisp2
# PCM5 ----> volume -----> iDisp3
# PCM6 ----> volume -----> iDisp4
# PCM7 --> volume --> demux --> SSP1  (Speaker - CODEC)


# if XPROC is not defined, define with default pipe
ifdef(`DMICPROC', , `define(DMICPROC, eq-iir-volume)')
ifdef(`RTNR',
`define(DMIC16KPROC, rtnr)',
`ifdef(`DMIC16KPROC', , `define(DMIC16KPROC, eq-iir-volume)')')

# Define pipeline id for sof-tgl-CODEC-rt5682.m4
# define pcm, pipeline and dai id
define(DMIC_PCM_48k_ID, `1')
define(DMIC_PIPELINE_48k_ID, `3')
define(DMIC_DAI_LINK_48k_ID, `1')
define(DMIC_PCM_16k_ID, `2')
define(DMIC_PIPELINE_16k_ID, `4')
define(DMIC_DAI_LINK_16k_ID, `2')
# define pcm, pipeline and dai id

# include the generic dmic
include(`platform/intel/intel-generic-dmic.m4')

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     frames, deadline, priority, core)

# Low Latency playback pipeline 2 on PCM 1 using max 2 channels of s24le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	1, 0, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency capture pipeline 3 on PCM 1 using max 2 channels of s24le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-capture.m4,
	2, 0, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 2 on PCM 2 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	5, 3, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 3 on PCM 3 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	6, 4, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 4 on PCM 4 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	7, 5, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 5 on PCM 5 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	8, 6, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 1 on PCM 0 using max 2 channels of s24le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
ifdef(`SPKPROC_FILTER1', `define(PIPELINE_FILTER1, SPKPROC_FILTER1)', `undefine(`PIPELINE_FILTER1')')
ifdef(`SPKPROC_FILTER2', `define(PIPELINE_FILTER2, SPKPROC_FILTER2)', `undefine(`PIPELINE_FILTER2')')
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	9, 7, 2, s32le,
	4000, 0, 0,
	48000, 48000, 48000)
undefine(`PIPELINE_FILTER1')
undefine(`PIPELINE_FILTER2')

# DAIs configuration
#

dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     frames, deadline, priority, core)

# playback DAI is SSP0 using 2 periods
# Buffers use s24le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	1, SSP, 0, SSP0-Codec,
	PIPELINE_SOURCE_1, 2, s24le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# capture DAI is SSP0 using 2 periods
# Buffers use s24le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	2, SSP, 0, SSP0-Codec,
	PIPELINE_SINK_2, 2, s24le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is iDisp1 using 2 periods
# Buffers use s32le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	5, HDA, 0, iDisp1,
	PIPELINE_SOURCE_5, 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is iDisp2 using 2 periods
# Buffers use s32le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	6, HDA, 1, iDisp2,
	PIPELINE_SOURCE_6, 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is iDisp3 using 2 periods
# Buffers use s32le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	7, HDA, 2, iDisp3,
	PIPELINE_SOURCE_7, 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is iDisp4 using 2 periods
# Buffers use s32le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	8, HDA, 3, iDisp4,
	PIPELINE_SOURCE_8, 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is SSP1 using 2 periods
# Buffers use s16le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	9, SSP, 1, SSP1-Codec,
	PIPELINE_SOURCE_9, 2, FMT,
	4000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# PCM Low Latency, id 0
dnl PCM_PLAYBACK_ADD(name, pcm_id, playback)
PCM_DUPLEX_ADD(Headset, 0, PIPELINE_PCM_1, PIPELINE_PCM_2)
PCM_PLAYBACK_ADD(HDMI1, 3, PIPELINE_PCM_5)
PCM_PLAYBACK_ADD(HDMI2, 4, PIPELINE_PCM_6)
PCM_PLAYBACK_ADD(HDMI3, 5, PIPELINE_PCM_7)
PCM_PLAYBACK_ADD(HDMI4, 6, PIPELINE_PCM_8)
PCM_PLAYBACK_ADD(Speakers, 7, PIPELINE_PCM_9)

#
# BE conf2igurations - overrides config in ACPI if present
#
dnl DAI_CONFIG(type, dai_index, link_id, name, ssp_config/dmic_config)
dnl SSP_CONFIG(format, mclk, bclk, fsync, tdm, ssp_config_data)
dnl SSP_CLOCK(clock, freq, codec_master, polarity)
dnl SSP_CONFIG_DATA(type, idx, valid bits, mclk_id)
dnl mclk_id is optional
dnl ssp1-maxmspk

# SSP 1 (ID: 7)
DAI_CONFIG(SSP, 1, 7, SSP1-Codec,
ifelse(
	CODEC, `MAX98357A', `
	SSP_CONFIG(I2S, SSP_CLOCK(mclk, 19200000, codec_mclk_in),
		SSP_CLOCK(bclk, 1536000, codec_slave),
		SSP_CLOCK(fsync, 48000, codec_slave),
		SSP_TDM(2, 16, 3, 3),
		SSP_CONFIG_DATA(SSP, 1, 16)))',
	CODEC, `RT1011', `
	SSP_CONFIG(DSP_A, SSP_CLOCK(mclk, 19200000, codec_mclk_in),
		SSP_CLOCK(bclk, 4800000, codec_slave),
		SSP_CLOCK(fsync, 48000, codec_slave),
		SSP_TDM(4, 25, 3, 15),
		SSP_CONFIG_DATA(SSP, 1, 24)))',
	)

# SSP 0 (ID: 0)
DAI_CONFIG(SSP, 0, 0, SSP0-Codec,
	SSP_CONFIG(I2S, SSP_CLOCK(mclk, 19200000, codec_mclk_in),
		SSP_CLOCK(bclk, 2400000, codec_slave),
		SSP_CLOCK(fsync, 48000, codec_slave),
		SSP_TDM(2, 25, 3, 3),
		SSP_CONFIG_DATA(SSP, 0, 24)))

# 4 HDMI/DP outputs (ID: 3,4,5,6)
DAI_CONFIG(HDA, 0, 3, iDisp1,
	HDA_CONFIG(HDA_CONFIG_DATA(HDA, 0, 48000, 2)))
DAI_CONFIG(HDA, 1, 4, iDisp2,
	HDA_CONFIG(HDA_CONFIG_DATA(HDA, 1, 48000, 2)))
DAI_CONFIG(HDA, 2, 5, iDisp3,
	HDA_CONFIG(HDA_CONFIG_DATA(HDA, 2, 48000, 2)))
DAI_CONFIG(HDA, 3, 6, iDisp4,
	HDA_CONFIG(HDA_CONFIG_DATA(HDA, 3, 48000, 2)))

DEBUG_END
