# Crossover Volume Pipeline
#
#  Low Latency Playback with demux and volume.
#
# Pipeline Endpoints for connection are :
#
#	B2 (DAI buffer)
#       Playback Crossover
#
# host PCM_P -- B0 --> Crossover 0 --> B1 ---> DAI0
#                                |
#                                pipeline n+1 --> DAI

# Include topology builder
include(`utils.m4')
include(`buffer.m4')
include(`pcm.m4')
include(`crossover.m4')
include(`bytecontrol.m4')

#
# Controls for Crossover
#

define(CROSSOVER_priv, concat(`crossover_bytes_', PIPELINE_ID))
define(MY_CROSSOVER_CTRL, concat(`crossover_control_', PIPELINE_ID))
include(`crossover_coef_default.m4')
C_CONTROLBYTES(MY_CROSSOVER_CTRL, PIPELINE_ID,
     CONTROLBYTES_OPS(bytes, 258 binds the control to bytes get/put handlers, 258, 258),
     CONTROLBYTES_EXTOPS(258 binds the control to bytes get/put handlers, 258, 258),
     , , ,
     CONTROLBYTES_MAX(, 1024),
     ,
     CROSSOVER_priv)

#
# Components and Buffers
#

# Host "Low latency Playback" PCM
# with 2 sink and 0 source periods
W_PCM_PLAYBACK(PCM_ID, Low Latency Playback, 2, 0)

# Crossover filter has 2 sink and source periods. TODO for controls
W_CROSSOVER(0, PIPELINE_FORMAT, 2, 2, LIST(`         ', "MY_CROSSOVER_CTRL"))

# Low Latency Buffers
W_BUFFER(0, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_HOST_MEM_CAP)
W_BUFFER(1, COMP_BUFFER_SIZE(DAI_PERIODS,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_COMP_MEM_CAP)


#
# Pipeline Graph
# host PCM_P -- B0 --> Crossover 0 --> B1 ---> DAI
#                                |
#                                pipeline n+1 --> DAI

P_GRAPH(pipe-crossover-playback, PIPELINE_ID,
	LIST(`		',
	`dapm(N_BUFFER(0), N_PCMP(PCM_ID))',
	`dapm(N_CROSSOVER(0), N_BUFFER(0))',
	`dapm(N_BUFFER(1), N_CROSSOVER(0))'))
#
# Pipeline Source and Sinks
#
indir(`define', concat(`PIPELINE_SOURCE_', PIPELINE_ID), N_BUFFER(1))
indir(`define', concat(`PIPELINE_CROSSOVER_', PIPELINE_ID), N_CROSSOVER(0))
indir(`define', concat(`PIPELINE_PCM_', PIPELINE_ID), Low Latency Playback PCM_ID)

#
# PCM Configuration
#


# PCM capabilities supported by FW
PCM_CAPABILITIES(Low Latency Playback PCM_ID, `S32_LE,S24_LE,S16_LE', 48000, 48000, 2, PIPELINE_CHANNELS, 2, 16, 192, 16384, 65536, 65536)

undefine(`MY_CROSSOVER_CTRL')
undefine(`CROSSOVER_priv')
