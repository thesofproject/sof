#  DAI capture connector
#
# Pipeline Endpoints for connection are :-
#
#	Source DAI
#       Demux
#
# Source DAI 0 --> B0 -> Demux
#

# Include topology builder
include(`utils.m4')
include(`muxdemux.m4')
include(`bytecontrol.m4')
include(`buffer.m4')
include(`dai.m4')
include(`pipeline.m4')

#
# Controls
#
# demux Bytes control with max value of 255
C_CONTROLBYTES(concat(`DEMUX', PIPELINE_ID), PIPELINE_ID,
	CONTROLBYTES_OPS(bytes, 258 binds the mixer control to bytes get/put handlers, 258, 258),
	CONTROLBYTES_EXTOPS(258 binds the mixer control to bytes get/put handlers, 258, 258),
	, , ,
	CONTROLBYTES_MAX(, 304),
	,	concat(`demux_priv_', PIPELINE_ID))

# Mux 0 has 2 sink and source periods.
W_MUXDEMUX(0, 1, PIPELINE_FORMAT, 2, 2, SCHEDULE_CORE,
	LIST(`         ', concat(`DEMUX', PIPELINE_ID)))

#
# DAI definitions
#
W_DAI_IN(DAI_TYPE, DAI_INDEX, DAI_BE, DAI_FORMAT, 0, DAI_PERIODS, SCHEDULE_CORE)

#
# DAI pipeline - always use 0 for DAIs - FIXME WHY 0?
#
W_PIPELINE(N_DAI_IN, SCHEDULE_PERIOD, SCHEDULE_PRIORITY, SCHEDULE_CORE, SCHEDULE_TIME_DOMAIN,
	pipe_dai_schedule_plat)

# Low Latency Buffers
W_BUFFER(0, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(DAI_FORMAT), DAI_CHANNELS, COMP_PERIOD_FRAMES(DAI_RATE, SCHEDULE_PERIOD)),
	PLATFORM_COMP_MEM_CAP)

#
# Graph connections to pipelines
#
P_GRAPH(pipe-dai-demux-capture, PIPELINE_ID,
	LIST(`		',
	`dapm(N_BUFFER(0), N_DAI_IN)',
	`dapm(N_MUXDEMUX(0), N_BUFFER(0))'))

indir(`define', concat(`PIPELINE_DEMUX_CAPTURE_SCHED_COMP_', PIPELINE_ID), N_DAI_IN)
indir(`define', concat(`PIPELINE_DEMUX_SOURCE_', PIPELINE_ID), N_MUXDEMUX(0))
