# Low Latency Pipeline
#
#  Low latency Capture PCM.
#
# Pipeline Endpoints for connection are :-
#
#	LL Playback Mixer (Mixer)
#	LL Capture Volume B4 (DAI buffer)
#	LL Playback Volume B3 (DAI buffer)
#
#
#  host PCM_P --B0--> volume(0P) --B1--+
#                                      |--ll mixer(M) --B2--> volume(LL) ---B3-->  sink DAI0
#                     pipeline n+1 >---+
#                                      |
#                     pipeline n+2 >---+
#                                      |
#                     pipeline n+3 >---+  .....etc....more pipes can be mixed here
#

# Include topology builder
include(`utils.m4')
include(`buffer.m4')
include(`dai.m4')
include(`pcm.m4')
#include(`pga.m4')
include(`muxdemux.m4')
include(`bytecontrol.m4')

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


#
# Components and Buffers
#

# Host "Low latency Playback" PCM
# with 2 sink and 0 source periods
W_PCM_CAPTURE(PCM_ID, Passthrough Capture, 0, 2, SCHEDULE_CORE)


# Mux 0 in MUX mode, has 2 sink and source periods.
#W_MUXDEMUX(name, mux0/demux1, format, periods_sink, periods_source, core, kcontrol_list)
#W_MUXDEMUX_AMD(0, 1, PIPELINE_FORMAT, 2, 4, SCHEDULE_CORE,
#	LIST(`         ', concat(`DEMUX', PIPELINE_ID)),SCHEDULE_TIME_DOMAIN,SCHED_COMP)
W_MUXDEMUX(0, 1, PIPELINE_FORMAT, 2, 4, SCHEDULE_CORE,
	LIST(`         ', concat(`DEMUX', PIPELINE_ID)))

#dnl W_BUFFER(name, Bufsize, capabilities)
# Low Latency Buffers
W_BUFFER(0, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_HOST_MEM_CAP)
W_BUFFER(1, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), 4,COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_COMP_MEM_CAP)


#
# Pipeline Graph
#
#  host PCM_P --B0--> volume(0P) --B1--+
#                                      |--ll mixer(M) --B2--> volume(LL) ---B3-->  sink DAI0
#                     pipeline n+1 >---+
#                                      |
#                     pipeline n+2 >---+
#                                      |
#                     pipeline n+3 >---+  .....etc....more pipes can be mixed here
#

P_GRAPH(pipe-low-latency-capture-demux, PIPELINE_ID,
	LIST(`		',
	`dapm(N_PCMC(PCM_ID), N_BUFFER(0))',
	`dapm(N_BUFFER(0), N_MUXDEMUX(0))',
	`dapm(N_MUXDEMUX(0), N_BUFFER(1))'))

#
# Pipeline Source and Sinks
#
indir(`define', concat(`PIPELINE_SINK_', PIPELINE_ID), N_BUFFER(1))
indir(`define', concat(`PIPELINE_DEMUX_', PIPELINE_ID), N_MUXDEMUX(0))
indir(`define', concat(`PIPELINE_PCM_', PIPELINE_ID), Passthrough Capture PCM_ID)

#W_PIPELINE(N_PCMC(PCM_ID), SCHEDULE_PERIOD, SCHEDULE_PRIORITY, SCHEDULE_CORE, SCHEDULE_TIME_DOMAIN, pipe_media_schedule_plat)
#W_PIPELINE(N_PCMC(PCM_ID), SCHEDULE_PERIOD, SCHEDULE_PRIORITY, SCHEDULE_CORE, SCHEDULE_TIME_DOMAIN, pipe_media_schedule_plat_dmic)


#
# PCM Configuration
#

# PCM capabilities supported by FW
PCM_CAPABILITIES(Passthrough Capture PCM_ID, COMP_FORMAT_NAME(PIPELINE_FORMAT), PCM_MIN_RATE, PCM_MAX_RATE, 2, PIPELINE_CHANNELS, 2, 16, 192, 16384, 65536, 65536)


undefine(`DEF_PGA_TOKENS')
undefine(`DEF_PGA_CONF')
