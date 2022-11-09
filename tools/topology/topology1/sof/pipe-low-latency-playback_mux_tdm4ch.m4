# Low Latency Pipeline
#
#  Low Latency Playback PCM mixed into single sink pipe.
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
W_PCM_PLAYBACK(PCM_ID, Low Latency Playback, 2, 0, SCHEDULE_CORE)


# Mux 0 in MUX mode, has 2 sink and source periods.
#W_MUXDEMUX(name, mux0/demux1, format, periods_sink, periods_source, core, kcontrol_list)
W_MUXDEMUX(0, 0, PIPELINE_FORMAT, 4, 2, SCHEDULE_CORE,
	LIST(`         ', concat(`DEMUX', PIPELINE_ID)))

#dnl W_BUFFER(name, Bufsize, capabilities)
# Low Latency Buffers
W_BUFFER(0, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_HOST_MEM_CAP)
#W_BUFFER(1, COMP_BUFFER_SIZE(4,
#	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS,COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
#	PLATFORM_COMP_MEM_CAP)
W_BUFFER(1, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), 4,COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_COMP_MEM_CAP)


#W_DAI_OUT(ACPSP1, 1, 1, PIPELINE_FORMAT, 2, 2, 0)
##W_DAI_OUT(DAI_TYPE, DAI_INDEX, DAI_BE, DAI_FORMAT, 0, DAI_PERIODS, SCHEDULE_CORE)

#DAI_CONFIG(ACPSP1, 1, 1, acp-amp-codec,
#	   ACPSP1_CONFIG(I2S, ACP_CLOCK(mclk, 49152000, codec_mclk_in),
#                ACP_CLOCK(bclk, 3072000, codec_slave),
#                ACP_CLOCK(fsync, 48000, codec_slave),
#                ACP_TDM(4, 32, 3, 3),ACPSP1_CONFIG_DATA(ACPSP1, 1, 48000, 4)))

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

P_GRAPH(pipe-low-latency-playback_mux_NewAMD, PIPELINE_ID,
	LIST(`		',
	`dapm(N_BUFFER(0), N_PCMP(PCM_ID))',
	`dapm(N_MUXDEMUX(0), N_BUFFER(0))',
	`dapm(N_BUFFER(1), N_MUXDEMUX(0))'))
	#`dapm(N_DAI_OUT, N_BUFFER(1))'))



#
# Pipeline Source and Sinks
#
indir(`define', concat(`PIPELINE_SOURCE_', PIPELINE_ID), N_BUFFER(1))
indir(`define', concat(`PIPELINE_DEMUX_', PIPELINE_ID), N_MUXDEMUX(0))
indir(`define', concat(`PIPELINE_PCM_', PIPELINE_ID), Low Latency Playback PCM_ID)

#
# PCM Configuration
#

# PCM capabilities supported by FW
PCM_CAPABILITIES(Low Latency Playback PCM_ID, CAPABILITY_FORMAT_NAME(PIPELINE_FORMAT), PCM_MIN_RATE, PCM_MAX_RATE, 2, PIPELINE_CHANNELS, 2, 16, 192, 16384, 65536, 65536)

undefine(`DEF_PGA_TOKENS')
undefine(`DEF_PGA_CONF')
