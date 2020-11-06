#
# Topology for Apollo Lake with direct attach digital microphones array
#

ifdef(`DMICSETTING', , `apl-dmic-a2b4')
include(`platform/intel/'DMICSETTING`.m4')
# Capture configuration
`# DAI0 'ACHANNEL`ch 'AFORMAT` mic1-2'
`# DAI1 'BCHANNEL`ch 'BFORMAT` mic1-4'

# Include topology builder
include(`utils.m4')
include(`dai.m4')
include(`pipeline.m4')

# Include TLV library
include(`common/tlv.m4')

# Include Token library
include(`sof/tokens.m4')

# Include Apollolake DSP configuration
include(`platform/intel/bxt.m4')
include(`platform/intel/dmic.m4')


#
# Define the pipelines
#
# PCM6 <--- volume <--- DMIC6 (DMIC01)
# PCM7 <--- volume <--- DMIC7 (DMIC16k)
#

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate,
dnl     time_domain, sched_comp)

`# Passthrough capture pipeline 6 on PCM 6 using max channels 'ACHANNEL
# Set 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-capture.m4,
	6, 6, ACHANNEL, AFORMAT,
	1000, 0, 0,
	ARATE, ARATE, ARATE)

`# Passthrough capture pipeline 7 on PCM 7 using max channels 'BCHANNEL
# Set 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-capture-16khz.m4,
	7, 7, BCHANNEL, BFORMAT,
	1000, 0, 0,
	BRATE, BRATE, BRATE)

#
# DAIs configuration
#

dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     deadline, priority, core, time_domain)

# capture DAI is DMIC 0 using 2 periods
`# Buffers use 'AFORMAT `format, 1000us deadline on core 0 with priority 0'
DAI_ADD(sof/pipe-dai-capture.m4,
	6, DMIC, 0, NoCodec-6,
	PIPELINE_SINK_6, 2, AFORMAT,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# capture DAI is DMIC 1 using 2 periods
`# Buffers use 'BFORMAT `format, 1000us deadline on core 0 with priority 0'
DAI_ADD(sof/pipe-dai-capture.m4,
	7, DMIC, 1, NoCodec-7,
	PIPELINE_SINK_7, 2, BFORMAT,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)


dnl PCM_DUPLEX_ADD(name, pcm_id, playback, capture)
dnl PCM_CAPTURE_ADD(name, pipeline, capture)
PCM_CAPTURE_ADD(DMIC, 6, PIPELINE_PCM_6)
PCM_CAPTURE_ADD(DMIC16kHz, 7, PIPELINE_PCM_7)

#
# BE configurations - overrides config in ACPI if present
#

dnl DAI_CONFIG(type, dai_index, link_id, name, ssp_config/dmic_config)

DAI_CONFIG(DMIC, 0, 6, NoCodec-6,
	   dnl DMIC_CONFIG(driver_version, clk_min, clk_mac, duty_min, duty_max,
	   dnl		   sample_rate, fifo word length, unmute time, type,
	   dnl 		   dai_index, pdm controller config)
	   DMIC_CONFIG(1, 2400000, 4800000, 40, 60, ARATE,
		DMIC_WORD_LENGTH(AFORMAT), 400, DMIC, 0,
		PDM_CONFIG(DMIC, 0, APDM)))

DAI_CONFIG(DMIC, 1, 7, NoCodec-7,
	   dnl DMIC_CONFIG(driver_version, clk_min, clk_mac, duty_min, duty_max,
	   dnl		   sample_rate, fifo word length, unmute time, type,
	   dnl             dai_index, pdm controller config)
	   DMIC_CONFIG(1, 2400000, 4800000, 40, 60, BRATE,
		DMIC_WORD_LENGTH(BFORMAT), 400, DMIC, 1,
		PDM_CONFIG(DMIC, 1, BPDM)))
