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

W_TONE(0)
W_PGA(Tone Volume)
W_BUFFER(0, BUF_INT_SIZE)
W_BUFFER(1, BUF_INT_SIZE)


#
# Pipeline Graph
#
#  Tone --B0--> volume --B1--> Endpoint Pipeline
#

SectionGraph."pipe-tone-PIPELINE_ID" {
	index STR(PIPELINE_ID)

	lines [
		dapm(N_BUFFER(0), N_TONE(0))
		dapm(N_PGA(Tone Volume), N_BUFFER(0))
		dapm(N_BUFFER(1), N_PGA(Tone Volume))
	]
}

#
# Pipeline Configuration.
#

W_PIPELINE(N_TONE(0), SCHEDULE_DEADLINE, pipe_tone_schedule_plat)
