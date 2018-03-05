# Low Latency Passthrough with volume Pipeline and PCM
#
# Pipeline Endpoints for connection are :-
#
#  host PCM_P --> B0 --> Volume 0 --> B1 --> sink DAI0

# Include topology builder
include(`local.m4')

#
# Controls
#
# Volume Mixer control with max value of 32
C_CONTROLMIXER(Master Playback Volume, PIPELINE_ID,
	CONTROLMIXER_OPS(volsw, 256 binds the mixer control to volume get/put handlers, 256, 256),
	CONTROLMIXER_MAX(, 32),
	false,
	CONTROLMIXER_TLV(TLV 32 steps from -90dB to +6dB for 3dB, vtlv_m90s3),
	Channel register and shift for Front Left/Right,
	LIST(`	', KCONTROL_CHANNEL(FL, 1, 0), KCONTROL_CHANNEL(FR, 1, 1)))

# Volume Mixer Switch control with max value of 1
C_CONTROLMIXER(Master Playback Volume Switch, PIPELINE_ID,
	CONTROLMIXER_OPS(volsw, 256 binds the mixer control to volume get/put handlers, 256, 256),
	CONTROLMIXER_MAX(max value 1 indicates switch type control, 1),
	false,
	,
	Channel register and shift for Front Left/Right,
	LIST(`	', KCONTROL_CHANNEL(FL, 1, 0), KCONTROL_CHANNEL(FR, 1, 1)))

#
# Components and Buffers
#

# Host "Passthrough Playback" PCM uses pipeline DMAC and channel
# with 2 sink and 0 source periods
W_PCM_PLAYBACK(Passthrough Playback, PIPELINE_DMAC, PIPELINE_DMAC_CHAN, 2, 0, 2)

# "Volume" has 2 source and 2 sink periods
W_PGA(0, PIPELINE_FORMAT, 2, 2, 2, LIST(`		', "Master Playback Volume Switch", "Master Playback Volume"))

# Playback Buffers
W_BUFFER(0, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, SCHEDULE_FRAMES),
	PLATFORM_HOST_MEM_CAP)
W_BUFFER(1, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(DAI_FORMAT), PIPELINE_CHANNELS, SCHEDULE_FRAMES),
	PLATFORM_DAI_MEM_CAP)

#
# DAI definitions
#
W_DAI_OUT(DAI_TYPE, DAI_INDEX, DAI_FORMAT, 0, DAI_PERIODS,
	DAI_PERIODS, dai0p_plat_conf)

#
# DAI pipeline - always use 0 for DAIs
#
W_PIPELINE(N_DAI_OUT, SCHEDULE_DEADLINE, SCHEDULE_PRIORITY, SCHEDULE_FRAMES,
	SCHEDULE_CORE, 0, pipe_dai_schedule_plat)

#
# Pipeline Graph
#
#  host PCM_P --> B0 --> Volume 0 --> B1 --> sink DAI0

P_GRAPH(pipe-pass-vol-playback-PIPELINE_ID, PIPELINE_ID,
	LIST(`		',
	`dapm(N_PCMP, Passthrough Playback PCM_ID)',
	`dapm(N_BUFFER(0), N_PCMP)',
	`dapm(N_PGA(0), N_BUFFER(0))',
	`dapm(N_BUFFER(1), N_PGA(0))'))

#
# Pipeline Source and Sinks
#
indir(`define', concat(`PIPELINE_SOURCE_', PIPELINE_ID), N_BUFFER(1))
indir(`define', concat(`PIPELINE_PCM_', PIPELINE_ID), Passthrough Playback PCM_ID)


#
# PCM Configuration
#

SectionPCMCapabilities.STR(Passthrough Playback PCM_ID) {

	formats "S32_LE,S24_LE,S16_LE"
	rate_min "48000"
	rate_max "48000"
	channels_min "2"
	channels_max "4"
	periods_min "2"
	periods_max "16"
	period_size_min	"192"
	period_size_max	"16384"
	buffer_size_min	"65536"
	buffer_size_max	"65536"
}

