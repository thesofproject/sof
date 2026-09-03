# Mixer DAI Playback connector
#
#  DAI playback starting with a LL mixer
#
# Pipeline Endpoints for connection are :-
#
#	LL Playback Mixer (Mixer)
#	LL Playback Volume B1 (DAI buffer)
#
# DAI_BUF --> ll mux(M) --> B0 -->  sink DAI
#
# the ll mixer is connected to one DAI_BUF by default. Additional ones can be added later

# Include topology builder
include(`utils.m4')
include(`muxdemux.m4')

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




# Mux 0 in MUX mode, has 2 sink and source periods.
#W_MUXDEMUX(name, mux0/demux1, format, periods_sink, periods_source, core, kcontrol_list)
W_MUXDEMUX(0, 0, PIPELINE_FORMAT, 4, 2, SCHEDULE_CORE,
	LIST(`         ', concat(`DEMUX', PIPELINE_ID)))


#
# DAI definitions
#
W_DAI_OUT(DAI_TYPE, DAI_INDEX, DAI_BE, DAI_FORMAT, 0, DAI_PERIODS, SCHEDULE_CORE)

#
# DAI pipeline - always use 0 for DAIs - FIXME WHY 0?
#
W_PIPELINE(N_DAI_OUT, SCHEDULE_PERIOD, SCHEDULE_PRIORITY, SCHEDULE_CORE, SCHEDULE_TIME_DOMAIN, pipe_dai_schedule_plat)

# Low Latency Buffers
W_BUFFER(0, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(DAI_FORMAT), DAI_CHANNELS, COMP_PERIOD_FRAMES(DAI_RATE, SCHEDULE_PERIOD)),
	PLATFORM_COMP_MEM_CAP)

#
# Graph connections to pipelines
# we don't connect 	`dapm(N_MIXER(0), DAI_BUF)' due to forward dependencies
#
P_GRAPH(DAI_NAME, PIPELINE_ID,
	LIST(`		',
	`dapm(N_BUFFER(0), N_MUXDEMUX(0))',
	`dapm(N_DAI_OUT, N_BUFFER(0))'))

indir(`define', concat(`PIPELINE_SOURCE_', PIPELINE_ID), N_BUFFER(0))
indir(`define', concat(`PIPELINE_PLAYBACK_SCHED_COMP_', PIPELINE_ID), N_DAI_OUT)
indir(`define', concat(`PIPELINE_MUX_', PIPELINE_ID), N_MUXDEMUX(0))

