# Passthrough Capture Pipeline with KPBM and PCM
#
# Pipeline Endpoints for connection are :-
#
#  host PCM_C <-+- KPBM 0 <-- B0 <-- source DAI0
#               |
#  Others    <--+

# Include topology builder
include(`utils.m4')
include(`buffer.m4')
include(`pcm.m4')
include(`dai.m4')
include(`kpbm.m4')
include(`enumcontrol.m4')
include(`pipeline.m4')

#
# Controls
#
# Volume Mixer control with max value of 32
# TODO pass in enum options
C_CONTROLENUM_TEXT(OUTPUTS, LIST(`		', STR(N_PCMC(PCM_ID)), STR(Other)))
C_CONTROLENUM(Capture Mux, PIPELINE_ID,
	CONTROLENUM_OPS(enum, 257 binds the mixer control to enum get/put handlers, 257, 257),
	LIST(`	', KCONTROL_ENUM_CHANNEL(FL, 0, 0), KCONTROL_ENUM_CHANNEL(FR, 0, 1)),
	OUTPUTS)

#
# Components and Buffers
#

# Host "Passthrough Capture" PCM
# with 0 sink and 2 source periods
W_PCM_CAPTURE(PCM_ID, KPBM Capture, 0, 2, 2)

# "KPBM" has 2 source and 2 sink periods
W_KPBM(0, PIPELINE_FORMAT, 2, 2, PIPELINE_ID)

# Capture Buffers
W_BUFFER(0, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, SCHEDULE_FRAMES),
	PLATFORM_HOST_MEM_CAP)
# Capture Buffers
W_BUFFER(1, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, SCHEDULE_FRAMES),
	PLATFORM_HOST_MEM_CAP)

#
# Pipeline Graph
#
#  host PCM_C <-- B1 <--+- KPBM 0 <-- B0 <-- source DAI0
#                	|
#            Others  <--

P_GRAPH(pipe-kpbm-capture-PIPELINE_ID, PIPELINE_ID,
	LIST(`		',
	`dapm(KPBM Capture PCM_ID, N_PCMC(PCM_ID))',
	`dapm(N_PCMC(PCM_ID), N_BUFFER(1))',
	`dapm(N_BUFFER(1), N_KPBM(0, PIPELINE_ID))',
	`dapm(N_KPBM(0, PIPELINE_ID), N_BUFFER(0))'))

#
# Pipeline Source and Sinks
# TODO add others for MUX (list ??)
#
indir(`define', concat(`PIPELINE_SINK_', PIPELINE_ID), N_BUFFER(0))
indir(`define', concat(`PIPELINE_SOURCE_', PIPELINE_ID), N_KPBM(0, PIPELINE_ID))
indir(`define', concat(`PIPELINE_PCM_', PIPELINE_ID), Mux Capture PCM_ID)

#
# PCM Configuration
#

PCM_CAPABILITIES(KPBM Capture PCM_ID, `S32_LE,S24_LE,S16_LE', 16000, 16000, 2, PIPELINE_CHANNELS, 2, 16, 192, 16384, 65536, 65536)

