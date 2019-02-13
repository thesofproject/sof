# Sound Detector
#
#  Generic sound detector.
#
# Pipeline Endpoints for connection are :-
#
#  (Sound Detector <-- Detect Switch) <-- Channel Mux <--- Source Pipeline
#

# Include topology builder
include(`utils.m4')
include(`buffer.m4')
include(`pga.m4')
include(`detect.m4')
include(`mixercontrol.m4')
include(`enumcontrol.m4')
include(`pipeline.m4')

#
# Controls
#

# Switch type Mixer Control with max value of 1 (Internal to Detector)
C_CONTROLENUM_TEXT(CHANNELS, LIST(`	        ', "Left", "Right"))
C_CONTROLMIXER(Detect Switch, PIPELINE_ID,
	CONTROLMIXER_OPS(volsw, 259 binds the mixer control to switch get/put handlers, 259, 259),
	CONTROLMIXER_MAX(max 1 indicates switch type control, 1),
	false,
	,
	Channel register and shift for Front Left/Right,
	LIST(`	', KCONTROL_CHANNEL(FL, 2, 0), KCONTROL_CHANNEL(FR, 2, 1)))

# Channel Selector Mux control with max value of 32 (Internal to Detector)
# TODO pass in enum options
C_CONTROLENUM(Channel Mux, PIPELINE_ID,
	CONTROLENUM_OPS(enum, 257 binds the mixer control to enum get/put handlers, 257, 257),
	LIST(`	', KCONTROL_ENUM_CHANNEL(FL, 0, 0), KCONTROL_ENUM_CHANNEL(FR, 0, 1)),
	CHANNELS)

#
# Components and Buffers
#

# "Detect 0" has 2 sink period and 0 source periods
W_DETECT(0, PIPELINE_FORMAT, 2, 0, KEYWORD,
	LIST(`		', "Detect Switch PIPELINE_ID"))

W_MUX(0, PIPELINE_FORMAT, 2, 2, SOF_COMP_MUX_CH_SEL, LIST(`		', "Channel Mux PIPELINE_ID"), PIPELINE_ID)

# Capture Buffers
W_BUFFER(0, COMP_BUFFER_SIZE(2,
	 COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, SCHEDULE_FRAMES),
	 PLATFORM_HOST_MEM_CAP)
# Capture Buffers
W_BUFFER(1, COMP_BUFFER_SIZE(2,
	 COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, SCHEDULE_FRAMES),
	 PLATFORM_HOST_MEM_CAP)

#FIXME: platform data in pipeline
W_PIPELINE(N_MUX(0, PIPELINE_ID), SCHEDULE_DEADLINE, SCHEDULE_PRIORITY, SCHEDULE_FRAMES, SCHEDULE_CORE, 0, pipe_media_schedule_plat)

#
# Pipeline Graph
#
# Detect <-- B0 <-- Mux <-- B1

P_GRAPH(pipe-detect-PIPELINE_ID, PIPELINE_ID,
	LIST(`		',
	`dapm(N_DETECT(0), N_BUFFER(0))',
	`dapm(N_BUFFER(0), N_MUX(0, PIPELINE_ID))',
	`dapm(N_MUX(0, PIPELINE_ID), N_BUFFER(1))'))

#
# Pipeline Source and Sinks
#
indir(`define', concat(`PIPELINE_SINK_', PIPELINE_ID), N_BUFFER(1))
