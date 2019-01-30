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
include(`pipeline.m4')

#
# Controls
#

# Switch type Mixer Control with max value of 1 (Internal to Detector)
C_CONTROLMIXER(Detect Switch, PIPELINE_ID,
	CONTROLMIXER_OPS(volsw, 259 binds the mixer control to switch get/put handlers, 259, 259),
	CONTROLMIXER_MAX(max 1 indicates switch type control, 1),
	false,
	,
	Channel register and shift for Front Left/Right,
	LIST(`	', KCONTROL_CHANNEL(FL, 2, 0), KCONTROL_CHANNEL(FR, 2, 1)))

# Channel Selector Mux control with max value of 32 (Internal to Detector)
# TODO pass in enum options
C_CONTROLMUX(Channel Mux, PIPELINE_ID,
	CONTROLMUX_OPS(enum, 256 binds the mixer control to volume get/put handlers, 256, 256),
	CONTROLMIXER_ITEMS(, 2),
	LIST(`	', KCONTROL_MUX_ENUM(FL, 0, 0), KCONTROL_MUX_ENUM(FR, 0, 1)),
	LIST(`  ', Left, Right))

#
# Components and Buffers
#

# "Detect 0" has 2 sink period and 0 source periods
W_DETECT(0, PIPELINE_FORMAT, 2, 0, KEYWORD,
	LIST(`		', "Detect Switch PIPELINE_ID"))

W_MUX(0, PIPELINE_FORMAT, 2, 2, LIST(`		', "Channel Mux PIPELINE_ID"))

#
# Pipeline Graph
#
# Detect <-- Mux <---

P_GRAPH(pipe-pass-mux-capture-PIPELINE_ID, PIPELINE_ID,
	LIST(`		',
	`dapm(N_DETECT(0), N_MUX(0))'))

#
# Pipeline Source and Sinks
#
indir(`define', concat(`PIPELINE_SINK_', PIPELINE_ID), N_MUX(0))

#
# Pipeline Configuration.
#

#W_PIPELINE(N_DETECT(0), SCHEDULE_DEADLINE, SCHEDULE_PRIORITY, SCHEDULE_FRAMES, SCHEDULE_CORE, 0, pipe_tone_schedule_plat)
