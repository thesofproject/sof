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
SectionControlMixer.STR(Master Playback Volume) {

	# control belongs to this index group
	index STR(PIPELINE_ID)

	# Channel register and shift for Front Left/Right
	channel."FL" {
		reg "1"
		shift "0"
	}
	channel."FR" {
		reg "1"
		shift "1"
	}

	# control uses bespoke driver get/put/info ID 0
	ops."ctl" {
		info "volsw"
		get "256"
		put "256"
	}

	# TLV 32 steps from -90dB to +6dB for 3dB
	max "32"
	invert "false"
	tlv "vtlv_m90s3"
}

SectionControlMixer.STR(Master Playback Volume Switch) {

	# control belongs to this index group
	index STR(PIPELINE_ID)

	# Channel register and shift for Front Left/Right
	channel."FL" {
		reg "1"
		shift "0"
	}
	channel."FR" {
		reg "1"
		shift "1"
	}

	# control uses bespoke driver get/put/info ID 0
	ops."ctl" {
		info "volsw"
		get "256"
		put "256"
	}

	# Volume mute on/off
	max "1"
	invert "false"
}
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

SectionGraph."pipe-pass-vol-playback-PIPELINE_ID" {
	index STR(PIPELINE_ID)

	lines [
		dapm(N_PCMP, Passthrough Playback PCM_ID)
		dapm(N_BUFFER(0), N_PCMP)
		dapm(N_PGA(0), N_BUFFER(0))
		dapm(N_BUFFER(1), N_PGA(0))
	]
}

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

