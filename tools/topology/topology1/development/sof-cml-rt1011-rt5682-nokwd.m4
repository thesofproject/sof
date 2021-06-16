#
# Topology for Cometlake with RT5682 codec and RT1011 class-D speaker amp
#

# Include topology builder
include(`abi.h')
include(`utils.m4')
include(`dai.m4')
include(`pipeline.m4')
include(`ssp.m4')
include(`hda.m4')

# Include TLV library
include(`common/tlv.m4')

# Include Token library
include(`sof/tokens.m4')

# Include Platform specific DSP configuration
include(`platform/intel/cml.m4')

# if XPROC is not defined, define with default pipe
ifdef(`HSMICPROC', , `define(HSMICPROC, volume)')
ifdef(`HSEARPROC', , `define(HSEARPROC, volume)')
ifdef(`SPKPROC', , `define(SPKPROC, volume)')
ifdef(`DMICPROC', , `define(DMICPROC, eq-iir-volume)')
ifdef(`DMIC16KPROC', , `define(DMIC16KPROC, eq-iir-volume)')

DEBUG_START

# Define pipeline id for intel-generic-dmic.m4
# to generate dmic setting
ifelse(CHANNELS, `0', ,
`
define(DMIC_PCM_48k_ID, `1')
define(DMIC_PIPELINE_48k_ID, `3')
define(DMIC_DAI_LINK_48k_ID, `1')
define(DMIC_PCM_16k_ID, `8')
define(DMIC_PIPELINE_16k_ID, `8')
define(DMIC_DAI_LINK_16k_ID, `2')
include(`platform/intel/intel-generic-dmic.m4')
'
)

#
# Define the pipelines
#
# PCM0 ----> HSEARPROC   (pipe 1) ----> SSP    (SSP_INDEX, BE link 0)
# PCM0 <---- HSMICPROC   (pipe 2) <---- SSP    (SSP_INDEX, BE link 0)
# PCM1 <---- DMICPROC    (pipe 3) <---- DMIC01 (dmic0 capture, , BE link 1)
# PCM2 ----> volume      (pipe 4) ----> iDisp1 (HDMI/DP playback, BE link 3)
# PCM3 ----> volume      (pipe 5) ----> iDisp2 (HDMI/DP playback, BE link 4)
# PCM4 ----> volume      (pipe 6) ----> iDisp3 (HDMI/DP playback, BE link 5)
# PCM5 ----> SPKPROC     (pipe 7) ----> SSP1   (speaker - rt1011, BE link 5)
# PCM8 <---- DMIC16KPROC (pipe 8) <---- DMIC1  (dmic16k, BE link 2)

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate,
dnl     time_domain, sched_comp)

# Low Latency playback pipeline 1 on PCM 0 using max 2 channels of s24le.
# Schedule 1000us deadline on core 0 with priority 0
ifdef(`HSEARPROC_FILTER1', `define(PIPELINE_FILTER1, HSEARPROC_FILTER1)', `undefine(`PIPELINE_FILTER1')')
ifdef(`HSEARPROC_FILTER2', `define(PIPELINE_FILTER2, HSEARPROC_FILTER2)', `undefine(`PIPELINE_FILTER2')')
PIPELINE_PCM_ADD(sof/pipe-HSEARPROC-playback.m4,
	1, 0, 2, s24le,
	1000, 0, 0,
	48000, 48000, 48000)
undefine(`PIPELINE_FILTER1')
undefine(`PIPELINE_FILTER2')

# Low Latency capture pipeline 2 on PCM 0 using max 2 channels of s24le.
# Schedule 1000us deadline on core 0 with priority 0
ifdef(`HSMICPROC_FILTER1', `define(PIPELINE_FILTER1, HSMICPROC_FILTER1)', `undefine(`PIPELINE_FILTER1')')
ifdef(`HSMICPROC_FILTER2', `define(PIPELINE_FILTER2, HSMICPROC_FILTER2)', `undefine(`PIPELINE_FILTER2')')
PIPELINE_PCM_ADD(sof/pipe-HSMICPROC-capture.m4,
	2, 0, 2, s24le,
	1000, 0, 0,
	48000, 48000, 48000)
undefine(`PIPELINE_FILTER1')
undefine(`PIPELINE_FILTER2')

# Low Latency playback pipeline 4 on PCM 2 using max 2 channels of s32le.
# Schedule 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	4, 2, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 5 on PCM 3 using max 2 channels of s32le.
# Schedule 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	5, 3, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 6 on PCM 4 using max 2 channels of s32le.
# Schedule 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	6, 4, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 7 on PCM 5 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
ifdef(`SPKPROC_FILTER1', `define(PIPELINE_FILTER1, SPKPROC_FILTER1)', `undefine(`PIPELINE_FILTER1')')
ifdef(`SPKPROC_FILTER2', `define(PIPELINE_FILTER2, SPKPROC_FILTER2)', `undefine(`PIPELINE_FILTER2')')
PIPELINE_PCM_ADD(sof/pipe-SPKPROC-playback.m4,
	7, 5, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)
undefine(`PIPELINE_FILTER1')
undefine(`PIPELINE_FILTER2')

#
# DAIs configuration
#

dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     deadline, priority, core, time_domain)

# playback DAI is SSP(SPP_INDEX) using 2 periods
# Buffers use s24le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	1, SSP, SSP_INDEX, SSP_NAME,
	PIPELINE_SOURCE_1, 2, s24le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# capture DAI is SSP(SSP_INDEX) using 2 periods
# Buffers use s24le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	2, SSP,SSP_INDEX, SSP_NAME,
	PIPELINE_SINK_2, 2, s24le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is SSP1 using 2 periods
# Buffers use s16le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	7, SSP, 1, SSP1-Codec,
	PIPELINE_SOURCE_7, 2, s24le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is iDisp1 using 2 periods
# Buffers use s32le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	4, HDA, 0, iDisp1,
	PIPELINE_SOURCE_4, 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is iDisp2 using 2 periods
# Buffers use s32le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	5, HDA, 1, iDisp2,
	PIPELINE_SOURCE_5, 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is iDisp3 using 2 periods
# Buffers use s32le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	6, HDA, 2, iDisp3,
	PIPELINE_SOURCE_6, 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# PCM Low Latency, id 0
dnl PCM_PLAYBACK_ADD(name, pcm_id, playback)
PCM_DUPLEX_ADD(Port1, 0, PIPELINE_PCM_1, PIPELINE_PCM_2)
PCM_PLAYBACK_ADD(HDMI1, 2, PIPELINE_PCM_4)
PCM_PLAYBACK_ADD(HDMI2, 3, PIPELINE_PCM_5)
PCM_PLAYBACK_ADD(HDMI3, 4, PIPELINE_PCM_6)
PCM_PLAYBACK_ADD(Speakers, 5, PIPELINE_PCM_7)

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

DAI_CONFIG(SSP, SSP1_INDEX, 6, SSP1_NAME,
        SSP_CONFIG(DSP_A, SSP_CLOCK(mclk, SSP1_MCLK_RATE, codec_mclk_in),
                SSP_CLOCK(bclk, 4800000, codec_slave),
                SSP_CLOCK(fsync, 48000, codec_slave),
                SSP_TDM(4, 25, 3, 15),
                SSP_CONFIG_DATA(SET_SSP1_CONFIG_DATA)))

# 3 HDMI/DP outputs (ID: 3,4,5)
DAI_CONFIG(HDA, 0, 3, iDisp1,
	HDA_CONFIG(HDA_CONFIG_DATA(HDA, 0, 48000, 2)))
DAI_CONFIG(HDA, 1, 4, iDisp2,
	HDA_CONFIG(HDA_CONFIG_DATA(HDA, 1, 48000, 2)))
DAI_CONFIG(HDA, 2, 5, iDisp3,
	HDA_CONFIG(HDA_CONFIG_DATA(HDA, 2, 48000, 2)))

DEBUG_END
