#
# Topology for GeminiLake with Dialog7219.
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
# PCM5  ----> Crossover
# PCM6  ----> Crossover
# PCM7  ----> Crossover
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

# currently this dai is here as "virtual" capture backend
W_DAI_IN(SSP, 10, SSP1-Codec, s32le, 3, 0)

# Capture pipeline 10 on PCM 5 using max 2 channels of s32le.
PIPELINE_PCM_ADD(sof/pipe-passthrough-capture-sched.m4,
	10, 5, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000,
	SCHEDULE_TIME_DOMAIN_TIMER)

# Capture pipeline 11 on PCM 6 using max 2 channels of s32le.
PIPELINE_PCM_ADD(sof/pipe-passthrough-capture-sched.m4,
￼	11, 6, 2, s32le,
￼	1000, 0, 0,
￼	48000, 48000, 48000,
￼	SCHEDULE_TIME_DOMAIN_TIMER)

# Capture pipeline 12 on PCM 7 using max 2 channels of s32le.
PIPELINE_PCM_ADD(sof/pipe-passthrough-capture-sched.m4,
	12, 7, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000,
	SCHEDULE_TIME_DOMAIN_TIMER)

# Connect demux to capture
SectionGraph."PIPE_CAP" {
	index "0"

	lines [
		# demux to capture
		dapm(PIPELINE_SINK_10, PIPELINE_CROSSOVER_1)
		# dapm(PIPELINE_SINK_11, PIPELINE_CROSSOVER_1)
		# dapm(PIPELINE_SINK_12, PIPELINE_CROSSOVER_1)
	]
}

# Connect virtual capture to dai
SectionGraph."PIPE_CAP_VIRT" {
	index "10"

	lines [
		dapm(ECHO REF 10, `SSP1.IN')
	]
}

# Connect virtual capture to dai
SectionGraph."PIPE_CAP_VIRT" {
	index "11"

	lines [
		# mux to capture
		dapm(ECHO REF 11, `SSP1.IN')
	]
}

# Connect virtual capture to dai
SectionGraph."PIPE_CAP_VIRT" {
	index "12"

	lines [
		# mux to capture
		dapm(ECHO REF 12, `SSP1.IN')
	]
}

PCM_PLAYBACK_ADD(Speakers, 0, PIPELINE_PCM_1)
PCM_CAPTURE_ADD(EchoRef, 10, PIPELINE_PCM_10)
PCM_CAPTURE_ADD(EchoRef2, 11, PIPELINE_PCM_11)
PCM_CAPTURE_ADD(EchoRef3, 12, PIPELINE_PCM_12)

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
