# Passthrough Capture Pipeline with MUX and PCM
#
# Pipeline Endpoints for connection are :-
#
#  host PCM_C <-+- Mux 0 <-- B0 <-- source DAI0
#               |
#  Others    <--+

# Include topology builder
include(`utils.m4')
include(`buffer.m4')
include(`pcm.m4')
include(`dai.m4')
include(`mux.m4')
include(`muxcontrol.m4')
include(`pipeline.m4')

#
# Controls
#
# Volume Mixer control with max value of 32
# TODO pass in enum options
C_CONTROLMUX(Detect Mux, PIPELINE_ID,
	CONTROLMUX_OPS(enum, 256 binds the mixer control to volume get/put handlers, 256, 256),
	CONTROLMIXER_ITEMS(, 2),
	LIST(`	', KCONTROL_MUX_ENUM(FL, 0, 0), KCONTROL_MUX_ENUM(FR, 0, 1)),
	LIST(`  ', N_PCMC(PCM_ID), Other))

#
# Components and Buffers
#

# Host "Passthrough Capture" PCM
# with 0 sink and 2 source periods
W_PCM_CAPTURE(PCM_ID, Keyword Detect, 0, 2, 2)

# "Mux" has 2 source and 2 sink periods
W_MUX_SMART(0, PIPELINE_FORMAT, 2, 2, 1, LIST(`		', "Detect Mux PIPELINE_ID"))

# Capture Buffers
W_BUFFER(0, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, SCHEDULE_FRAMES),
	PLATFORM_HOST_MEM_CAP)

#
# Pipeline Graph
#
#  host PCM_C <-+- Mux 0 <-- B0 <-- source DAI0
#               |
#  Others    <--+

P_GRAPH(pipe-detect-mux-capture-PIPELINE_ID, PIPELINE_ID,
	LIST(`		',
	`dapm(Mux Capture PCM_ID, N_PCMC(PCM_ID))',
	`dapm(N_PCMC(PCM_ID), N_MUX(0))',
	`dapm(N_MUX(0), N_BUFFER(0))'))

#
# Pipeline Source and Sinks
# TODO add others for MUX (list ??)
#
indir(`define', concat(`PIPELINE_SINK_', PIPELINE_ID), N_BUFFER(0))
indir(`define', concat(`PIPELINE_SOURCE_', PIPELINE_ID), N_MUX(0))
indir(`define', concat(`PIPELINE_PCM_', PIPELINE_ID), Keyword Detect PCM_ID)

#
# PCM Configuration
#

PCM_CAPABILITIES(Mux Capture PCM_ID, `S32_LE,S24_LE,S16_LE', 16000, 16000, 2, PIPELINE_CHANNELS, 2, 16, 192, 16384, 65536, 65536)

