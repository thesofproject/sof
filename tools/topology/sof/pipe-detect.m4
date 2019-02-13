# Sound Detector
#
#  Generic sound detector.
#
# Pipeline Endpoints for connection are :-
#
#  (Sound Detector <-- Channel Selector) <-- Key Phrase Buffer Manager <--- Source Pipeline
#

# Include topology builder
include(`utils.m4')
include(`buffer.m4')
include(`pga.m4')
include(`ch_sel.m4')
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

#
# Components and Buffers
#

# "Detect 0" has 2 sink period and 0 source periods
W_DETECT(0, PIPELINE_FORMAT, 2, 0, KEYWORD,
	LIST(`		', "Detect Switch PIPELINE_ID"))

W_SELECTOR(0, PIPELINE_FORMAT, 2, 2, 2, 1, 0)

# Capture Buffers
W_BUFFER(1, COMP_BUFFER_SIZE(2,
	 COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, SCHEDULE_FRAMES),
	 PLATFORM_HOST_MEM_CAP)
# Capture Buffers
W_BUFFER(2, COMP_BUFFER_SIZE(2,
	 COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, SCHEDULE_FRAMES),
	 PLATFORM_HOST_MEM_CAP)

#FIXME: platform data in pipeline
W_PIPELINE(N_SELECTOR(0), SCHEDULE_DEADLINE, SCHEDULE_PRIORITY, SCHEDULE_FRAMES, SCHEDULE_CORE, 0, pipe_media_schedule_plat)

#
# Pipeline Graph
#
# Detect 0 <-- B2 <-- Channel Selector 0 <-- B1

P_GRAPH(pipe-detect-PIPELINE_ID, PIPELINE_ID,
	LIST(`		',
	`dapm(N_DETECT(0), N_BUFFER(2))',
	`dapm(N_BUFFER(2), N_SELECTOR(0))',
	`dapm(N_SELECTOR(0), N_BUFFER(1))'))

#
# Pipeline Source and Sinks
#
indir(`define', concat(`PIPELINE_SINK_', PIPELINE_ID), N_BUFFER(1))
