#
# Topology for Apollo Lake with direct attach digital microphones array
#

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

define(DMIC_PDM_CONFIG, ifelse(CHANNELS, `4', ``FOUR_CH_PDM0_PDM1'',
	`ifelse(CHANNELS, `2', ``STEREO_PDM0'', `')'))

#
# Define the pipelines
#
# CPROC is for capture pipeline processing, i.e volume, eq, eq-iir, eq-fir etc
# In this file, volume and eq are used. If anything else is used, please update
# pipeline comments below for tracking purpose.
#
ifelse(CPROC, `volume', `# PCM6 <----- DMIC6 (DMIC01)',
    `ifelse(CPROC, `eq', `# PCM6 <---- EQ IIR <----- DMIC6 (DMIC01)', `')')

# The pipeline naming notation is pipe-PROCESSING-DIRECTION.m4
define(PIPE_PROC_CAPTURE, `sof/pipe-`CPROC'-capture.m4')

# PCM7 <----- DMIC7 (DMIC16k)
#

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate,
dnl     time_domain, sched_comp)

# Passthrough capture pipeline 6 on PCM 6 using max channels defined by CHANNELS.
# Set 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(PIPE_PROC_CAPTURE,
	6, 6, CHANNELS, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Passthrough capture pipeline 7 on PCM 7 using max channels defined by CHANNELS.
# Set 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-capture.m4,
	7, 7, CHANNELS, s32le,
	1000, 0, 0,
	16000, 16000, 16000)

#
# DAIs configuration
#

dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     deadline, priority, core, time_domain)

# capture DAI is DMIC 0 using 2 periods
# Buffers use s32le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	6, DMIC, 0, NoCodec-6,
	PIPELINE_SINK_6, 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# capture DAI is DMIC 1 using 2 periods
# Buffers use s16le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	7, DMIC, 1, NoCodec-7,
	PIPELINE_SINK_7, 2, s16le,
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
	   dnl             dai_index, pdm controller config)
	   DMIC_CONFIG(1, 2400000, 4800000, 40, 60, 48000,
		DMIC_WORD_LENGTH(s32le), 400, DMIC, 0,
		PDM_CONFIG(DMIC, 0, DMIC_PDM_CONFIG)))

DAI_CONFIG(DMIC, 1, 7, NoCodec-7,
	   dnl DMIC_CONFIG(driver_version, clk_min, clk_mac, duty_min, duty_max,
	   dnl		   sample_rate, fifo word length, unmute time, type,
	   dnl             dai_index, pdm controller config)
	   DMIC_CONFIG(1, 2400000, 4800000, 40, 60, 16000,
		DMIC_WORD_LENGTH(s16le), 400, DMIC, 1,
		PDM_CONFIG(DMIC, 1, DMIC_PDM_CONFIG)))
