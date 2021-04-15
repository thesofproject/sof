#
# Topology for GeminiLake with Dialog Semiconductor DA7219 or
#                              Cirrus Logic CS42L42
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

# Include bxt DSP configuration
include(`platform/intel/bxt.m4')
include(`platform/intel/dmic.m4')

#
# Define the pipelines
#
# PCM0  ----> volume (pipe 1)   -----> SSP1 (speaker - maxim98357a, BE link 0)
`# PCM1  <---> volume (pipe 2,3) <----> SSP2 (headset - 'HEADPHONE`, BE link 1)'
`# PCM99 <---- 'DMICPROC` <---- DMIC0 (dmic capture, BE link 2)'
# PCM5  ----> volume (pipe 5)   -----> iDisp1 (HDMI/DP playback, BE link 3)
# PCM6  ----> Volume (pipe 6)   -----> iDisp2 (HDMI/DP playback, BE link 4)
# PCM7  ----> volume (pipe 7)   -----> iDisp3 (HDMI/DP playback, BE link 5)
#

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate,
dnl     time_domain, sched_comp)

# Low Latency playback pipeline 1 on PCM 0 using max 2 channels of s32le.
# 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	1, 0, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 2 on PCM 1 using max 2 channels of s32le.
# 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	2, 1, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency capture pipeline 3 on PCM 1 using max 2 channels of s32le.
# 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-capture.m4,
	3, 1, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# if DMICPROC is not defined, default pipepline is volume
ifdef(`DMICPROC', , `define(DMICPROC, volume)')
define(DEF_PIPE_DMIC_CAPTURE, sof/pipe-DMICPROC-capture.m4)

# Low Latency capture pipeline 4 on PCM 99 using max 4 channels of s32le.
# 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(DEF_PIPE_DMIC_CAPTURE,
	4, 99, 4, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 5 on PCM 5 using max 2 channels of s32le.
# 1000us deadline on core 0 with priority 0
# PIPELINE_PCM_ADD(sof/pipe-passthrough-playback.m4,
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
        5, 5, 2, s32le,
        1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 6 on PCM 6 using max 2 channels of s32le.
# 1000us deadline on core 0 with priority 0
# PIPELINE_PCM_ADD(sof/pipe-passthrough-playback.m4,
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
        6, 6, 2, s32le,
        1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 7 on PCM 7 using max 2 channels of s32le.
# 1000us deadline on core 0 with priority 0
# PIPELINE_PCM_ADD(sof/pipe-passthrough-playback.m4,
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
dnl     deadline, priority, core, time_domain)

# playback DAI is SSP1 using 2 periods
# Buffers use s16le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	1, SSP, 1, SSP1-Codec,
	PIPELINE_SOURCE_1, 2, s16le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is SSP2 using 2 periods
# Buffers use s16le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	2, SSP, 2, SSP2-Codec,
	PIPELINE_SOURCE_2, 2, s16le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# capture DAI is SSP2 using 2 periods
# Buffers use s16le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	3, SSP, 2, SSP2-Codec,
	PIPELINE_SINK_3, 2, s16le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# capture DAI is DMIC0 using 2 periods
# Buffers use s32le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	4, DMIC, 0, dmic01,
	PIPELINE_SINK_4, 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is iDisp1 using 2 periods
# Buffers use s32le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
        5, HDA, 3, iDisp1,
        PIPELINE_SOURCE_5, 2, s32le,
        1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is iDisp2 using 2 periods
# Buffers use s32le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
        6, HDA, 4, iDisp2,
        PIPELINE_SOURCE_6, 2, s32le,
        1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is iDisp3 using 2 periods
# Buffers use s32le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
        7, HDA, 5, iDisp3,
        PIPELINE_SOURCE_7, 2, s32le,
        1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

PCM_PLAYBACK_ADD(Speakers, 0, PIPELINE_PCM_1)
PCM_DUPLEX_ADD(Headset, 1, PIPELINE_PCM_2, PIPELINE_PCM_3)
PCM_CAPTURE_ADD(DMIC, 99, PIPELINE_PCM_4)
PCM_PLAYBACK_ADD(HDMI1, 5, PIPELINE_PCM_5)
PCM_PLAYBACK_ADD(HDMI2, 6, PIPELINE_PCM_6)
PCM_PLAYBACK_ADD(HDMI3, 7, PIPELINE_PCM_7)

#
# BE configurations - overrides config in ACPI if present
#

#SSP 1 (ID: 0) with 19.2 MHz mclk with MCLK_ID 1 (unused), 1.536 MHz blck
DAI_CONFIG(SSP, 1, 0, SSP1-Codec,
	SSP_CONFIG(I2S, SSP_CLOCK(mclk, 19200000, codec_mclk_in),
		SSP_CLOCK(bclk, 1536000, codec_slave),
		SSP_CLOCK(fsync, 48000, codec_slave),
		SSP_TDM(2, 16, 3, 3),
		SSP_CONFIG_DATA(SSP, 1, 16, 1)))

ifelse(HEADPHONE, `da7219', `
#SSP 2 (ID: 1) with 19.2 MHz mclk with MCLK_ID 1, 1.92 MHz bclk
DAI_CONFIG(SSP, 2, 1, SSP2-Codec,
	SSP_CONFIG(I2S, SSP_CLOCK(mclk, 19200000, codec_mclk_in),
		SSP_CLOCK(bclk, 1920000, codec_slave),
		SSP_CLOCK(fsync, 48000, codec_slave),
		SSP_TDM(2, 20, 3, 3),
		SSP_CONFIG_DATA(SSP, 2, 16, 1)))
', HEADPHONE, `cs42l42', `
#SSP 2 (ID: 1) with 19.2 MHz mclk with MCLK_ID 1 (unused), 3.072 MHz bclk, no quirk, 10 ms BCLK delay
DAI_CONFIG(SSP, 2, 1, SSP2-Codec,
	SSP_CONFIG(I2S, SSP_CLOCK(mclk, 19200000, codec_mclk_in),
		SSP_CLOCK(bclk, 3072000, codec_slave),
		SSP_CLOCK(fsync, 48000, codec_slave),
		SSP_TDM(2, 32, 3, 3),
		SSP_CONFIG_DATA(SSP, 2, 16, 1, 0, 10)))
', )

# dmic01 (id: 2)
DAI_CONFIG(DMIC, 0, 2, dmic01,
	DMIC_CONFIG(1, 2400000, 4800000, 40, 60, 48000,
		DMIC_WORD_LENGTH(s32le), 400, DMIC, 0,
		PDM_CONFIG(DMIC, 0, FOUR_CH_PDM0_PDM1)))

# 3 HDMI/DP outputs (ID: 3,4,5)
DAI_CONFIG(HDA, 3, 3, iDisp1,
	HDA_CONFIG(HDA_CONFIG_DATA(HDA, 3, 48000, 2)))
DAI_CONFIG(HDA, 4, 4, iDisp2,
	HDA_CONFIG(HDA_CONFIG_DATA(HDA, 4, 48000, 2)))
DAI_CONFIG(HDA, 5, 5, iDisp3,
	HDA_CONFIG(HDA_CONFIG_DATA(HDA, 5, 48000, 2)))

## remove warnings with SST hard-coded routes

VIRTUAL_WIDGET(ssp1 Tx, out_drv, 0)
VIRTUAL_WIDGET(ssp2 Rx, out_drv, 1)
VIRTUAL_WIDGET(ssp2 Tx, out_drv, 2)
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
