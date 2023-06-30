#
# Topology for Tigerlake RVP board CI testing, with covering as more
# features as possible running with no codec machine.
#
# TGL Host GW DMAC support max 6 playback and max 6 capture channels so some
# pipelines/PCMs/DAIs are commented out to keep within HW bounds. If these
# are needed then they can be used provided other PCMs/pipelines/SSPs are
# commented out in their place.

# Include topology builder
include(`utils.m4')
include(`dai.m4')
include(`ssp.m4')
include(`crossover.m4')
include(`bytecontrol.m4')
include(`pipeline.m4')

# Include Token library
include(`sof/tokens.m4')

# Include Tigerlake DSP configuration
include(`platform/intel/tgl.m4')

#define SSP_MCLK
define(`SSP_MCLK', 38400000)

#
# Topology
#
# pcm0p --> buf1.0 --> crossover1.0 --> buf1.1 --> ssp0.out
#                          |
#                          +----------> buf2.0 --> ssp1.out
#                          |
#                          +----------> buf3.0 --> ssp2.out
#                          |
#                          +----------> buf4.0 --> ssp3.out
#
# pcm0c <-- buf5.0 <-- ssp0.in
# pcm1c <-- buf6.0 <-- ssp1.in
# pcm2c <-- buf7.0 <-- ssp2.in
# pcm3c <-- buf8.0 <-- ssp3.in
#

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate,
dnl     time_domain, sched_comp, dynamic)

define(PIPELINE_FILTER1, crossover/coef_4way_48000_200_1000_3000_1_2_3_4.m4)

# Low Latency playback pipeline 1 on PCM 0 using max 2 channels of s16le.
# Schedule 48 frames per 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-crossover-playback.m4,
	1, 0, 2, s16le,
	1000, 0, 0,
	48000, 48000, 48000,
	SCHEDULE_TIME_DOMAIN_TIMER,
	PIPELINE_PLAYBACK_SCHED_COMP_1)

undefine(`PIPELINE_FILTER1')

# Playback pipeline 2 on PCM 1 using max 2 channels of s16le.
# Set 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-dai-endpoint.m4,
	2, 1, 2, s16le,
	1000, 0, 0,
	48000, 48000, 48000,
	SCHEDULE_TIME_DOMAIN_TIMER)

# Playback pipeline 3 on PCM 2 using max 2 channels of s16le.
# Set 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-dai-endpoint.m4,
	3, 2, 2, s16le,
	1000, 0, 0,
	48000, 48000, 48000,
	SCHEDULE_TIME_DOMAIN_TIMER)

# Playback pipeline 4 on PCM 3 using max 2 channels of s16le.
# Set 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-dai-endpoint.m4,
	4, 3, 2, s16le,
	1000, 0, 0,
	48000, 48000, 48000,
	SCHEDULE_TIME_DOMAIN_TIMER)

# connect pipelines together
SectionGraph."pipe-sof-2-pipe" {
        index "2"

        lines [
		# connect the second sink buffer
                dapm(PIPELINE_SOURCE_2, PIPELINE_CROSSOVER_1)
        ]
}

# connect pipelines together
SectionGraph."pipe-sof-3-pipe" {
        index "3"

        lines [
		# connect the second sink buffer
                dapm(PIPELINE_SOURCE_3, PIPELINE_CROSSOVER_1)
        ]
}

# connect pipelines together
SectionGraph."pipe-sof-4-pipe" {
        index "4"

        lines [
		# connect the second sink buffer
                dapm(PIPELINE_SOURCE_4, PIPELINE_CROSSOVER_1)
        ]
}

# Pass-through capture pipeline 5 on PCM 0 using max 2 channels of s16le.
# Schedule 48 frames per 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-passthrough-capture.m4,
	5, 0, 2, s16le,
	1000, 0, 0,
	48000, 48000, 48000)

# Pass-throug capture pipeline 6 on PCM 1 using max 2 channels of s16le.
# Schedule 48 frames per 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-passthrough-capture.m4,
	6, 1, 2, s16le,
	1000, 0, 0,
	48000, 48000, 48000)

# Pass-throug capture pipeline 7 on PCM 2 using max 2 channels of s16le.
# Schedule 48 frames per 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-passthrough-capture.m4,
	7, 2, 2, s16le,
	1000, 0, 0,
	48000, 48000, 48000)

# Pass-throug capture pipeline 6 on PCM 3 using max 2 channels of s16le.
# Schedule 48 frames per 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-passthrough-capture.m4,
	8, 3, 2, s16le,
	1000, 0, 0,
	48000, 48000, 48000)

#
# DAIs configuration
#

dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     period , priority, core, time_domain,
dnl     channels, rate, dynamic_pipe)

# playback DAI is SSP0 using 2 periods
# Buffers use s16le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	1, SSP, 0, NoCodec-0,
	PIPELINE_SOURCE_1, 2, s16le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER,
	2, 48000)



dnl DAI_ADD_SCHED(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     period , priority, core, time_domain, sched_comp)

# playback DAI is SSP1 using 2 periods
# Buffers use s16le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD_SCHED(sof/pipe-dai-sched-playback.m4,
	2, SSP, 1, NoCodec-1,
	PIPELINE_SOURCE_2, 2, s16le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER,
	PIPELINE_PLAYBACK_SCHED_COMP_1)

# playback DAI is SSP2 using 2 periods
# Buffers use s16le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD_SCHED(sof/pipe-dai-sched-playback.m4,
	3, SSP, 2, NoCodec-2,
	PIPELINE_SOURCE_3, 2, s16le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER,
	PIPELINE_PLAYBACK_SCHED_COMP_1)

# playback DAI is SSP3 using 2 periods
# Buffers use s16le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD_SCHED(sof/pipe-dai-sched-playback.m4,
	4, SSP, 3, NoCodec-3,
	PIPELINE_SOURCE_4, 2, s16le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER,
	PIPELINE_PLAYBACK_SCHED_COMP_1)



# capture DAI is SSP0 using 2 periods
# Buffers use s16le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	5, SSP, 0, NoCodec-0,
	PIPELINE_SINK_5, 2, s16le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# capture DAI is SSP1 using 2 periods
# Buffers use s16le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	6, SSP, 1, NoCodec-1,
	PIPELINE_SINK_6, 2, s16le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# capture DAI is SSP2 using 2 periods
# Buffers use s16le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	7, SSP, 2, NoCodec-2,
	PIPELINE_SINK_7, 2, s16le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# capture DAI is SSP3 using 2 periods
# Buffers use s16le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	8, SSP, 3, NoCodec-3,
	PIPELINE_SINK_8, 2, s16le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)


dnl PCM_DUPLEX_ADD(name, pcm_id, playback, capture)
PCM_DUPLEX_ADD(Port0, 0, PIPELINE_PCM_1, PIPELINE_PCM_5)

dnl PCM_CAPTURE_ADD(name, pcm_id, capture)
PCM_CAPTURE_ADD(Port1, 1, PIPELINE_PCM_6)
PCM_CAPTURE_ADD(Port2, 2, PIPELINE_PCM_7)
PCM_CAPTURE_ADD(Port3, 3, PIPELINE_PCM_8)

#
# BE configurations - overrides config in ACPI if present
#

dnl DAI_CONFIG(type, idx, link_id, name, ssp_config/dmic_config)
DAI_CONFIG(SSP, 0, 0, NoCodec-0,
		SSP_CONFIG(I2S, SSP_CLOCK(mclk, 38400000, codec_mclk_in),
			SSP_CLOCK(bclk, 2400000, codec_slave),
			SSP_CLOCK(fsync, 48000, codec_slave),
			SSP_TDM(2, 25, 3, 3),
			SSP_CONFIG_DATA(SSP, 0, 24, 0, SSP_QUIRK_LBM)))

DAI_CONFIG(SSP, 1, 1, NoCodec-1,
		SSP_CONFIG(I2S, SSP_CLOCK(mclk, 38400000, codec_mclk_in),
			SSP_CLOCK(bclk, 2400000, codec_slave),
			SSP_CLOCK(fsync, 48000, codec_slave),
			SSP_TDM(2, 25, 3, 3),
			SSP_CONFIG_DATA(SSP, 1, 24, 0, SSP_QUIRK_LBM)))

DAI_CONFIG(SSP, 2, 2, NoCodec-2,
		SSP_CONFIG(I2S, SSP_CLOCK(mclk, 38400000, codec_mclk_in),
			SSP_CLOCK(bclk, 2400000, codec_slave),
			SSP_CLOCK(fsync, 48000, codec_slave),
			SSP_TDM(2, 25, 3, 3),
			SSP_CONFIG_DATA(SSP, 2, 24, 0, SSP_QUIRK_LBM)))

DAI_CONFIG(SSP, 3, 3, NoCodec-3,
		SSP_CONFIG(I2S, SSP_CLOCK(mclk, 38400000, codec_mclk_in),
			SSP_CLOCK(bclk, 2400000, codec_slave),
			SSP_CLOCK(fsync, 48000, codec_slave),
			SSP_TDM(2, 25, 3, 3),
			SSP_CONFIG_DATA(SSP, 3, 24, 0, SSP_QUIRK_LBM)))
