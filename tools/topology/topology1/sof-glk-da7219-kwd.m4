#
ifelse(PLATFORM, `glk', `# Topology for GeminiLake with Dialog7219+Maxim98357a.', `')
ifelse(PLATFORM, `cml', `# Topology for CometLake with Dialog7219+Maxim98357a.', `')
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

# include platform specific dsp configuration and machine specific settings
include(`platform/intel/'PLATFORM`-da7219.m4')

define(DMIC_16k_PCM_NAME, `DMIC16k')

define(KWD_PIPE_SCH_DEADLINE_US, 20000)

ifelse(CODEC, `MAX98390',`
define(`SSP1_VALID_BITS_STR', `s32le')
define(`SSP1_BCLK', 3072000)
define(`SSP1_VALID_BITS', 32)')

#
# Define the pipelines
#
# PCM0  ----> volume (pipe 1)   -----> SSP1 (speaker - maxim98357a, BE link 0)
# PCM1  <---> volume (pipe 2,3) <----> SSP(SSP_INDEX) (headset - dailog7219, BE link 1)
# PCM(DMIC_PCM_NUM) <---- DMIC0 (dmic capture, BE link 2)
ifelse(CODEC, `MAX98390',`
# PCM4  <---- passthrough (pipe 10)  <----- SSP1 (echoref - maxim98390, BE link 0)')
# PCM5  ----> volume (pipe 5)   -----> iDisp1 (HDMI/DP playback, BE link 3)
# PCM6  ----> volume (pipe 6)   -----> iDisp2 (HDMI/DP playback, BE link 4)
# PCM7  ----> volume (pipe 7)   -----> iDisp3 (HDMI/DP playback, BE link 5)
# PCM8  <-------(pipe 8) <------------+- KPBM 0 <----- DMIC1 (dmic16k, BE link 6)
#                                     |
# Detector <--- selector (pipe 9) <---+
#

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate,
dnl     time_domain, sched_comp)

# Low Latency playback pipeline 1 on PCM 0 using max 2 channels of s32le.
# Set 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(PIPE_VOLUME_PLAYBACK,
	1, 0, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 2 on PCM 1 using max 2 channels of s32le.
# Set 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	2, 1, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency capture pipeline 3 on PCM 1 using max 2 channels of s32le.
# Set 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-capture.m4,
	3, 1, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency capture pipeline 4 on PCM 99 using max 4 channels of s32le.
# Set 1000us deadline on core 0 with priority 0

PIPELINE_PCM_ADD(DMIC_PIPE_CAPTURE,
	4, DMIC_PCM_NUM, 4, DMIC01_FMT,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 5 on PCM 5 using max 2 channels of s32le.
# Set 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
        5, 5, 2, s32le,
        1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 6 on PCM 6 using max 2 channels of s32le.
# Set 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
        6, 6, 2, s32le,
        1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 7 on PCM 7 using max 2 channels of s32le.
# Set 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
        7, 7, 2, s32le,
        1000, 0, 0,
	48000, 48000, 48000)

ifelse(CODEC, `MAX98390',`
# Capture pipeline 10 on PCM 4 using max 2 channels of s32le.
PIPELINE_PCM_ADD(sof/pipe-passthrough-capture.m4,
        10, 4, 2, s32le,
        1000, 0, 0,
        48000, 48000, 48000)')

#
# DAIs configuration
#

dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     deadline, priority, core, time_domain)

# playback DAI is SSP1 using 2 periods
# Buffers use s16le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	1, SSP, 1, SSP1-Codec,
	PIPELINE_SOURCE_1, 2, SSP1_VALID_BITS_STR,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

ifelse(CODEC, `MAX98390',`
# Capture DAI is SSP1 using 2 periods
# Buffers use s24le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
        10, SSP, 1, SSP1-Codec,
        PIPELINE_SINK_10, 2, SSP1_VALID_BITS_STR,
        1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)')

# playback DAI is SSP(SSP_INDEX) using 2 periods
# Buffers use s16le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	2, SSP, SSP_INDEX, SSP_NAME,
	PIPELINE_SOURCE_2, 2, s16le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# capture DAI is SSP(SSP_INDEX) using 2 periods
# Buffers use s16le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	3, SSP, SSP_INDEX, SSP_NAME,
	PIPELINE_SINK_3, 2, s16le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# capture DAI is DMIC0 using 2 periods
# Buffers use s32le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	4, DMIC, 0, dmic01,
	PIPELINE_SINK_4, 2, DMIC01_FMT,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is iDisp1 using 2 periods
# Buffers use s32le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
        5, HDA, HDMI0_INDEX, iDisp1,
        PIPELINE_SOURCE_5, 2, s32le,
        1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is iDisp2 using 2 periods
# Buffers use s32le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
        6, HDA, HDMI1_INDEX, iDisp2,
        PIPELINE_SOURCE_6, 2, s32le,
        1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is iDisp3 using 2 periods
# Buffers use s32le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
        7, HDA, HDMI2_INDEX, iDisp3,
        PIPELINE_SOURCE_7, 2, s32le,
        1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

#
# KWD configuration
#

# Passthrough capture pipeline 8 on PCM 8 using max 2 channels.
# Schedule 20000us deadline on core 0 with priority 0
PIPELINE_PCM_DAI_ADD(sof/pipe-kfbm-capture.m4,
	8, 8, 2, DMIC1_FMT,
	KWD_PIPE_SCH_DEADLINE_US, 0, 0, DMIC, 1, DMIC01_FMT, 3,
	16000, 16000, 16000)

# capture DAI is DMIC 1 using 3 periods
# Buffers use s16le format, with 320 frame per 20000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	8, DMIC, 1, dmic16k,
	PIPELINE_SINK_8, 3, DMIC01_FMT,
	KWD_PIPE_SCH_DEADLINE_US, 0, 0,
	SCHEDULE_TIME_DOMAIN_TIMER)


PCM_PLAYBACK_ADD(Speakers, 0, PIPELINE_PCM_1)
PCM_DUPLEX_ADD(Headset, 1, PIPELINE_PCM_2, PIPELINE_PCM_3)
PCM_CAPTURE_ADD(DMIC, DMIC_PCM_NUM, PIPELINE_PCM_4)
PCM_PLAYBACK_ADD(HDMI1, 5, PIPELINE_PCM_5)
PCM_PLAYBACK_ADD(HDMI2, 6, PIPELINE_PCM_6)
PCM_PLAYBACK_ADD(HDMI3, 7, PIPELINE_PCM_7)
ifelse(CODEC, `MAX98390',`
PCM_CAPTURE_ADD(EchoRef, 4, PIPELINE_PCM_10)')

# keyword detector pipe
dnl PIPELINE_ADD(pipeline,
dnl     pipe id, max channels, format,
dnl     period, priority, core,
dnl     sched_comp, time_domain,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate)
PIPELINE_ADD(sof/pipe-detect.m4,
	     9, 2, DMIC1_FMT,
	     KWD_PIPE_SCH_DEADLINE_US, 1, 0,
	     PIPELINE_SCHED_COMP_8, SCHEDULE_TIME_DOMAIN_TIMER,
	     16000, 16000, 16000)

# Connect pipelines together
SectionGraph."pipe-sof-PLATFORM-keyword-detect" {
        index "0"

        lines [
		# keyword detect
                dapm(PIPELINE_SINK_9, PIPELINE_SOURCE_8)
		dapm(PIPELINE_PCM_8, PIPELINE_DETECT_9)
        ]
}

#
# BE configurations - overrides config in ACPI if present
#

DAI_CONFIG(SSP, 1, 0, SSP1-Codec,
        SSP_CONFIG(I2S, SSP_CLOCK(mclk, SSP_MCLK_RATE, codec_mclk_in),
                SSP_CLOCK(bclk, SSP1_BCLK, codec_slave),
                SSP_CLOCK(fsync, SSP_FSYNC, codec_slave),
                SSP_TDM(2, SSP1_VALID_BITS, 3, 3),
                SSP_CONFIG_DATA(SSP, 1, SSP1_VALID_BITS, MCLK_ID)))

DAI_CONFIG(SSP, SSP_INDEX, 1, SSP_NAME,
        SSP_CONFIG(I2S, SSP_CLOCK(mclk, SSP_MCLK_RATE, codec_mclk_in),
                SSP_CLOCK(bclk, SSP_BCLK, codec_slave),
                SSP_CLOCK(fsync, SSP_FSYNC, codec_slave),
                SSP_TDM(2, SSP_BITS_WIDTH, 3, 3),
                SSP_CONFIG_DATA(SSP, SSP_INDEX, SSP_VALID_BITS, MCLK_ID)))

# dmic01 (ID: 2)
DAI_CONFIG(DMIC, 0, 2, dmic01,
	DMIC_CONFIG(1, 2400000, 4800000, 40, 60, 48000,
		DMIC_WORD_LENGTH(DMIC01_FMT), 400, DMIC, 0,
		PDM_CONFIG(DMIC, 0, FOUR_CH_PDM0_PDM1)))

# 3 HDMI/DP outputs (ID: 3,4,5)
DAI_CONFIG(HDA, HDMI0_INDEX, 3, iDisp1,
	HDA_CONFIG(HDA_CONFIG_DATA(HDA, HDMI0_INDEX, 48000, 2)))
DAI_CONFIG(HDA, HDMI1_INDEX, 4, iDisp2,
	HDA_CONFIG(HDA_CONFIG_DATA(HDA, HDMI1_INDEX, 48000, 2)))
DAI_CONFIG(HDA, HDMI2_INDEX, 5, iDisp3,
	HDA_CONFIG(HDA_CONFIG_DATA(HDA, HDMI2_INDEX, 48000, 2)))

# dmic16k (ID: 6)
DAI_CONFIG(DMIC, 1, 6, dmic16k,
           DMIC_CONFIG(1, 2400000, 4800000, 40, 60, 16000,
                DMIC_WORD_LENGTH(DMIC01_FMT), 400, DMIC, 1,
                PDM_CONFIG(DMIC, 1, STEREO_PDM0)))

## remove warnings with SST hard-coded routes

VIRTUAL_WIDGET(UNUSED_SSP_ROUTE1 Tx, out_drv, 0)
VIRTUAL_WIDGET(UNUSED_SSP_ROUTE2 Rx, out_drv, 1)
VIRTUAL_WIDGET(UNUSED_SSP_ROUTE2 Tx, out_drv, 2)
VIRTUAL_WIDGET(iDisp3 Tx, out_drv, 15)
VIRTUAL_WIDGET(iDisp2 Tx, out_drv, 16)
VIRTUAL_WIDGET(iDisp1 Tx, out_drv, 17)
VIRTUAL_WIDGET(DMIC01 Rx, out_drv, 3)
VIRTUAL_WIDGET(DMic, out_drv, 4)
VIRTUAL_WIDGET(dmic01_hifi, out_drv, 5)
VIRTUAL_WIDGET(hif5-0 Output, out_drv, 6)
VIRTUAL_WIDGET(hif6-0 Output, out_drv, 7)
VIRTUAL_WIDGET(hif7-0 Output, out_drv, 8)
VIRTUAL_WIDGET(iDisp3_out, out_drv, 9)
VIRTUAL_WIDGET(iDisp2_out, out_drv, 10)
VIRTUAL_WIDGET(iDisp1_out, out_drv, 11)
VIRTUAL_WIDGET(codec0_out, output, 12)
VIRTUAL_WIDGET(codec1_out, output, 13)
VIRTUAL_WIDGET(codec0_in, input, 14)
