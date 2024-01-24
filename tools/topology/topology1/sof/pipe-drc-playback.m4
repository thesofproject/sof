# Dynamic Range Compressor with volume Pipeline and PCM
#
# Pipeline Endpoints for connection are :-
#
#  host PCM_P --> B0 --> DRC --> B1 --> sink DAI0

# Include topology builder
include(`utils.m4')
include(`buffer.m4')
include(`pcm.m4')
include(`dai.m4')
include(`bytecontrol.m4')
include(`pipeline.m4')
include(`drc.m4')

#
# Controls
#

#
# Components and Buffers
#

# Host "DRC Playback" PCM
# with 2 sink and 0 source periods
W_PCM_PLAYBACK(PCM_ID, DRC Playback, 2, 0, SCHEDULE_CORE)

#
# DRC Configuration
#

define(DRC_priv, concat(`drc_bytes_', PIPELINE_ID))
define(MY_DRC_CTRL, concat(`drc_control_', PIPELINE_ID))
include(`drc_coef_generic_notebook_speaker.m4')
C_CONTROLBYTES(MY_DRC_CTRL, PIPELINE_ID,
      CONTROLBYTES_OPS(bytes, 258 binds the control to bytes get/put handlers, 258, 258),
      CONTROLBYTES_EXTOPS(258 binds the control to bytes get/put handlers, 258, 258),
      , , ,
      CONTROLBYTES_MAX(, 1024),
      ,
      DRC_priv)

# "DRC" has 2 sink periods and 2 source periods
W_DRC(0, PIPELINE_FORMAT, 2, 2, SCHEDULE_CORE,
	LIST(`   ', "MY_DRC_CTRL"))

# Playback Buffers
W_BUFFER(0, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS,
	COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_HOST_MEM_CAP)
W_BUFFER(1, COMP_BUFFER_SIZE(DAI_PERIODS,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS,
	COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_DAI_MEM_CAP)

#
# Pipeline Graph
#
#  host PCM_P --> B0 --> DRC --> B1 --> sink DAI0

P_GRAPH(pipe-drc-playback, PIPELINE_ID,
	LIST(`		',
	`dapm(N_BUFFER(0), N_PCMP(PCM_ID))',
	`dapm(N_DRC(0), N_BUFFER(0))',
	`dapm(N_BUFFER(1), N_DRC(0))'))

#
# Pipeline Source and Sinks
#
indir(`define', concat(`PIPELINE_SOURCE_', PIPELINE_ID), N_BUFFER(1))
indir(`define', concat(`PIPELINE_PCM_', PIPELINE_ID), DRC Playback PCM_ID)

#
# PCM Configuration
#

PCM_CAPABILITIES(DRC Playback PCM_ID, CAPABILITY_FORMAT_NAME(PIPELINE_FORMAT), PCM_MIN_RATE,
	PCM_MAX_RATE, 2, PIPELINE_CHANNELS, 2, 16, 192, 16384, 65536, 65536)

undefine(`MY_DRC_CTRL')
undefine(`DRC_priv')
