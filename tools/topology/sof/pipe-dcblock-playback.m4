# Low Latency Passthrough with volume Pipeline and PCM
#
# Pipeline Endpoints for connection are :-
#
#  host PCM_P --> B0 --> DC Block --> B1 --> sink DAI0

# Include topology builder
include(`utils.m4')
include(`buffer.m4')
include(`pcm.m4')
include(`dai.m4')
include(`bytecontrol.m4')
include(`pipeline.m4')
include(`dcblock.m4')

#
# Controls
#

#
# Components and Buffers
#

# Host "DC Block Playback" PCM
# with 2 sink and 0 source periods
W_PCM_PLAYBACK(PCM_ID, DC Block Playback, 2, 0, SCHEDULE_CORE)

#
# DC Block Configuration
#

define(DCBLOCK_priv, concat(`dcblock_bytes_', PIPELINE_ID))
define(MY_DCBLOCK_CTRL, concat(`dcblock_control_', PIPELINE_ID))
include(`dcblock_coef_default.m4')
# DC Block Bytes control with max value of 156
# The max size needs to also take into account the space required to hold the control data IPC message
# struct sof_ipc_ctrl_data requires 92 bytes
# DCBLOCK priv in dcblock_coef_default.m4 (ABI header (32 bytes) + 8 dwords) requires 64 bytes
# Therefore at least 156 bytes are required for this kcontrol
# Any value lower than that would end up in a topology load error
 C_CONTROLBYTES(MY_DCBLOCK_CTRL, PIPELINE_ID,
      CONTROLBYTES_OPS(bytes, 258 binds the control to bytes get/put handlers, 258, 258),
      CONTROLBYTES_EXTOPS(258 binds the control to bytes get/put handlers, 258, 258),
      , , ,
      CONTROLBYTES_MAX(, 156),
      ,
      DCBLOCK_priv)

# "DC Block" has 2 sink periods and 2 source periods
W_DCBLOCK(0, PIPELINE_FORMAT, 2, 2, SCHEDULE_CORE,
	LIST(`   ', "MY_DCBLOCK_CTRL"))

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
#  host PCM_P --> B0 --> DC Block --> B1 --> sink DAI0

P_GRAPH(pipe-dcblock-playback, PIPELINE_ID,
	LIST(`		',
	`dapm(N_BUFFER(0), N_PCMP(PCM_ID))',
	`dapm(N_DCBLOCK(0), N_BUFFER(0))',
	`dapm(N_BUFFER(1), N_DCBLOCK(0))'))

#
# Pipeline Source and Sinks
#
indir(`define', concat(`PIPELINE_SOURCE_', PIPELINE_ID), N_BUFFER(1))
indir(`define', concat(`PIPELINE_PCM_', PIPELINE_ID), DC Block Playback PCM_ID)

#
# PCM Configuration
#

PCM_CAPABILITIES(DC Block Playback PCM_ID, CAPABILITY_FORMAT_NAME(PIPELINE_FORMAT), PCM_MIN_RATE,
	PCM_MAX_RATE, 2, PIPELINE_CHANNELS, 2, 16, 192, 16384, 65536, 65536)

undefine(`MY_DCBLOCK_CTRL')
undefine(`DCBLOCK_priv')
