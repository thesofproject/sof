# Low Power PCM Media Pipeline
#
#  Low power PCM media playback with SRC and volume.
#
# Pipeline Endpoints for connection are :-
#
#  host PCM_P --B0--> volume(0P) --B1--> SRC -- B2 --> Endpoint Pipeline
#

# Include topology builder
include(`utils.m4')
include(`src.m4')
include(`buffer.m4')
include(`pga.m4')
include(`mixercontrol.m4')
include(`pipeline.m4')
include(`pcm.m4')

#
# Controls
#
# Volume Mixer control with max value of 32
C_CONTROLMIXER(PCM PCM_ID Playback Volume, PIPELINE_ID,
	CONTROLMIXER_OPS(volsw, 256 binds the mixer control to volume get/put handlers, 256, 256),
	CONTROLMIXER_MAX(, 32),
	false,
	CONTROLMIXER_TLV(TLV 32 steps from -64dB to 0dB for 2dB, vtlv_m64s2),
	Channel register and shift for Front Left/Right,
	VOLUME_CHANNEL_MAP)

#
# SRC Configuration
#

W_VENDORTUPLES(media_src_tokens, sof_src_tokens, LIST(`		', `SOF_TKN_SRC_RATE_OUT	"PIPELINE_RATE"'))

W_DATA(media_src_conf, media_src_tokens)

#
# Volume Configuration
#

W_VENDORTUPLES(playback_pga_tokens, sof_volume_tokens,
LIST(`		', `SOF_TKN_VOLUME_RAMP_STEP_TYPE	"0"'
     `		', `SOF_TKN_VOLUME_RAMP_STEP_MS		"250"'))

W_DATA(playback_pga_conf, playback_pga_tokens)

#
# Components and Buffers
#

# Host "Media Playback" PCM
# with 2 sink and 0 source periods
W_PCM_PLAYBACK(PCM_ID, Media Playback, 2, 0, SCHEDULE_CORE)

# "Playback Volume" has 3 sink period and 2 source periods for host ping-pong
W_PGA(0, PIPELINE_FORMAT, 3, 2, playback_pga_conf, SCHEDULE_CORE,
	LIST(`		', "PIPELINE_ID PCM PCM_ID Playback Volume"))

# "SRC 0" has 3 sink and source periods.
W_SRC(0, PIPELINE_FORMAT, 3, 3, media_src_conf)

# Media Source Buffers to SRC
W_BUFFER(0, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_HOST_MEM_CAP)
W_BUFFER(1,COMP_BUFFER_SIZE(3,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_COMP_MEM_CAP)

# Buffer B2 is on fixed rate sink side of SRC.
W_BUFFER(2, COMP_BUFFER_SIZE(3,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_COMP_MEM_CAP)

#
# Pipeline Graph
#
#  PCM --B0--> volume --B1--> SRC --> B2 --> Endpoint Pipeline
#

P_GRAPH(pipe-media-PIPELINE_ID, PIPELINE_ID,
	LIST(`		',
	`dapm(N_BUFFER(0), N_PCMP(PCM_ID))',
	`dapm(N_PGA(0), N_BUFFER(0))',
	`dapm(N_BUFFER(1), N_PGA(0))',
	`dapm(N_SRC(0), N_BUFFER(1))'
	`dapm(N_BUFFER(2), N_SRC(0))'))

#
# Pipeline Source and Sinks
#
indir(`define', concat(`PIPELINE_SOURCE_', PIPELINE_ID), N_BUFFER(2))

#
# Pipeline Configuration.
#

W_PIPELINE(SCHED_COMP, SCHEDULE_PERIOD, SCHEDULE_PRIORITY, SCHEDULE_CORE, SCHEDULE_TIME_DOMAIN, pipe_media_schedule_plat)

#
# PCM Configuration
#

# PCM capabilities supported by FW

PCM_CAPABILITIES(Media Playback PCM_ID, CAPABILITY_FORMAT_NAME(PIPELINE_FORMAT),
	PCM_MIN_RATE, PCM_MAX_RATE, 2, PIPELINE_CHANNELS,
	2, 16, 192, 16384, 65536, 65536)

# PCM Media Playback
SectionPCM.STR(Media Playback PCM_ID) {

	index STR(PIPELINE_ID)

	# used for binding to the PCM
	id STR(PCM_ID)

	dai.STR(Media Playback PCM_ID) {
		id STR(PCM_ID)
	}

	# Playback Configuration
	pcm."playback" {

		capabilities STR(Media Playback PCM_ID)
	}
}
