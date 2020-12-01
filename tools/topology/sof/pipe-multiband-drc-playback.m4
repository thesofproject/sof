# Multiband DRC with volume Pipeline and PCM
#
# Pipeline Endpoints for connection are :-
#
#  host PCM_P --> B0 --> MULTIBAND_DRC --> B1 --> sink DAI0

# Include topology builder
include(`utils.m4')
include(`buffer.m4')
include(`pcm.m4')
include(`dai.m4')
include(`bytecontrol.m4')
include(`pipeline.m4')
include(`multiband_drc.m4')

#
# Controls
#

#
# Components and Buffers
#

# Host "Multiband DRC Playback" PCM
# with 2 sink and 0 source periods
W_PCM_PLAYBACK(PCM_ID, Multiband DRC Playback, 2, 0, SCHEDULE_CORE)

#
# Multiband DRC Configuration
#

define(MULTIBAND_DRC_priv, concat(`multiband_drc_bytes_', PIPELINE_ID))
define(MY_MULTIBAND_DRC_CTRL, concat(`multiband_drc_control_', PIPELINE_ID))
include(`multiband_drc_coef_default.m4')
C_CONTROLBYTES(MY_MULTIBAND_DRC_CTRL, PIPELINE_ID,
      CONTROLBYTES_OPS(bytes, 258 binds the control to bytes get/put handlers, 258, 258),
      CONTROLBYTES_EXTOPS(258 binds the control to bytes get/put handlers, 258, 258),
      , , ,
      CONTROLBYTES_MAX(, 1024),
      ,
      MULTIBAND_DRC_priv)

# "MULTIBAND_DRC" has 2 sink periods and 2 source periods
W_MULTIBAND_DRC(0, PIPELINE_FORMAT, 2, 2, SCHEDULE_CORE,
	LIST(`   ', "MY_MULTIBAND_DRC_CTRL"))

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
#  host PCM_P --> B0 --> MULTIBAND_DRC --> B1 --> sink DAI0

P_GRAPH(pipe-multiband-drc-playback-PIPELINE_ID, PIPELINE_ID,
	LIST(`		',
	`dapm(N_BUFFER(0), N_PCMP(PCM_ID))',
	`dapm(N_MULTIBAND_DRC(0), N_BUFFER(0))',
	`dapm(N_BUFFER(1), N_MULTIBAND_DRC(0))'))

#
# Pipeline Source and Sinks
#
indir(`define', concat(`PIPELINE_SOURCE_', PIPELINE_ID), N_BUFFER(1))
indir(`define', concat(`PIPELINE_PCM_', PIPELINE_ID), Multiband DRC Playback PCM_ID)

#
# PCM Configuration
#

PCM_CAPABILITIES(Multiband DRC Playback PCM_ID, CAPABILITY_FORMAT_NAME(PIPELINE_FORMAT), PCM_MIN_RATE,
	PCM_MAX_RATE, 2, PIPELINE_CHANNELS, 2, 16, 192, 16384, 65536, 65536)

undefine(`MY_MULTIBAND_DRC_CTRL')
undefine(`MULTIBAND_DRC_priv')
