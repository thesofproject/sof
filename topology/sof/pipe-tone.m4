# Tone Generator
#
#  Multi Frequency Tone Generator.
#
# Pipeline Endpoints for connection are :-
#
#  Tone --B0--> volume --B1--> Endpoint Pipeline
#

# Include topology builder
include(`local.m4')

#
# Controls
#

SectionControlMixer.STR(Tone Volume PIPELINE_ID) {

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
		# 256 binds the mixer control to volume get/put handlers
		get "256"
		put "256"
	}

	# TLV 32 steps from -90dB to +6dB for 3dB
	max "32"
	invert "false"
	tlv "vtlv_m90s3"
}

#
# Components and Buffers
#

# "Tone 0" has 2 sink period and 0 source periods
W_TONE(0, PIPELINE_FORMAT, 2, 0, 0)

# "Tone Volume" has 2 sink period and 2 source periods
W_PGA(0, PIPELINE_FORMAT, 2, 2, 0, LIST(`		', "Tone Volume PIPELINE_ID"))

# Low Latency Buffers
W_BUFFER(0,COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, SCHEDULE_FRAMES),
	PLATFORM_COMP_MEM_CAP)
W_BUFFER(1, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, SCHEDULE_FRAMES),
	PLATFORM_COMP_MEM_CAP)


#
# Pipeline Graph
#
#  Tone --B0--> volume --B1--> Endpoint Pipeline
#

SectionGraph."pipe-tone-PIPELINE_ID" {
	index STR(PIPELINE_ID)

	lines [
		dapm(N_BUFFER(0), N_TONE(0))
		dapm(N_PGA(0), N_BUFFER(0))
		dapm(N_BUFFER(1), N_PGA(0))
	]
}

#
# Pipeline Source and Sinks
#
indir(`define', concat(`PIPELINE_SOURCE_', PIPELINE_ID), N_BUFFER(1))

#
# Pipeline Configuration.
#

W_PIPELINE(N_TONE(0), SCHEDULE_DEADLINE, SCHEDULE_PRIORITY, SCHEDULE_FRAMES, SCHEDULE_CORE, 1, pipe_tone_schedule_plat)
