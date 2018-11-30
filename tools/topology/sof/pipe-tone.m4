# Tone Generator
#
#  Multi Frequency Tone Generator.
#
# Pipeline Endpoints for connection are :-
#
#  Tone --B0--> volume --B1--> Endpoint Pipeline
#

# Include topology builder
include(`utils.m4')
include(`buffer.m4')
include(`pga.m4')
include(`tone.m4')
include(`mixercontrol.m4')
include(`pipeline.m4')

#
# Controls
#

# Volume Mixer control with max value of 32
C_CONTROLMIXER(Tone Volume, PIPELINE_ID,
	CONTROLMIXER_OPS(volsw, 256 binds the mixer control to volume get/put handlers, 256, 256),
	CONTROLMIXER_MAX(, 32),
	false,
	CONTROLMIXER_TLV(TLV 32 steps from -90dB to +6dB for 3dB, vtlv_m90s3),
	Channel register and shift for Front Left/Right,
	LIST(`	', KCONTROL_CHANNEL(FL, 1, 0), KCONTROL_CHANNEL(FR, 1, 1)))

# Switch type Mixer Control with max value of 1
C_CONTROLMIXER(Tone Switch, PIPELINE_ID,
	CONTROLMIXER_OPS(volsw, 259 binds the mixer control to switch get/put handlers, 259, 259),
	CONTROLMIXER_MAX(max 1 indicates switch type control, 1),
	false,
	,
	Channel register and shift for Front Left/Right,
	LIST(`	', KCONTROL_CHANNEL(FL, 2, 0), KCONTROL_CHANNEL(FR, 2, 1)))

#
# Components and Buffers
#

# "Tone 0" has 2 sink period and 0 source periods
W_TONE(0, PIPELINE_FORMAT, 2, 0, 0, LIST(`		', "PIPELINE_ID Tone Switch"))

# "Tone Volume" has 2 sink period and 2 source periods
W_PGA(0, PIPELINE_FORMAT, 2, 2, 0, LIST(`		', "PIPELINE_ID Tone Volume"))

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

P_GRAPH(pipe-tone-PIPELINE_ID, PIPELINE_ID,
	LIST(`		',
	`dapm(N_BUFFER(0), N_TONE(0))',
	`dapm(N_PGA(0), N_BUFFER(0))',
	`dapm(N_BUFFER(1), N_PGA(0))'))

#
# Pipeline Source and Sinks
#
indir(`define', concat(`PIPELINE_SOURCE_', PIPELINE_ID), N_BUFFER(1))

#
# Pipeline Configuration.
#

W_PIPELINE(N_TONE(0), SCHEDULE_DEADLINE, SCHEDULE_PRIORITY, SCHEDULE_FRAMES, SCHEDULE_CORE, 0, pipe_tone_schedule_plat)
