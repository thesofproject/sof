# Capture pipeline part with volume processing and demux
#
# Pipeline Endpoints for connection are :-
#
#  demux <-- B0 <-- Volume 0 <-- B1

# Include topology builder
include(`utils.m4')
include(`buffer.m4')
include(`pcm.m4')
include(`pga.m4')
include(`dai.m4')
include(`pipeline.m4')
include(`bytecontrol.m4')
include(`mixercontrol.m4')
include(`muxdemux.m4')


#
# Controls
#

# Volume Mixer control with max value of 32
C_CONTROLMIXER(Capture Volume, PIPELINE_ID,
	CONTROLMIXER_OPS(volsw,
		256 binds the mixer control to volume get/put handlers,
		256, 256),
	CONTROLMIXER_MAX(, 70),
	false,
	CONTROLMIXER_TLV(TLV 80 steps from -50dB to +20dB for 1dB, vtlv_m50s1),
	Channel register and shift for Front Left/Right,
	VOLUME_CHANNEL_MAP)

# demux Bytes control with max value of 255
C_CONTROLBYTES(concat(`DEMUX', PIPELINE_ID), PIPELINE_ID,
	CONTROLBYTES_OPS(bytes, 258 binds the mixer control to bytes get/put handlers, 258, 258),
	CONTROLBYTES_EXTOPS(258 binds the mixer control to bytes get/put handlers, 258, 258),
	, , ,
	CONTROLBYTES_MAX(, 304),
	,	concat(`demux_priv_', PIPELINE_ID))

# Volume Configuration
define(DEF_PGA_TOKENS, concat(`pga_tokens_', PIPELINE_ID))
define(DEF_PGA_CONF, concat(`pga_conf_', PIPELINE_ID))

W_VENDORTUPLES(DEF_PGA_TOKENS, sof_volume_tokens,
LIST(`		', `SOF_TKN_VOLUME_RAMP_STEP_TYPE	"0"'
     `		', `SOF_TKN_VOLUME_RAMP_STEP_MS		"250"'))

W_DATA(DEF_PGA_CONF, DEF_PGA_TOKENS)

#
# Components and Buffers
#

# "Volume" has 2 source and 2 sink periods
W_PGA(0, PIPELINE_FORMAT, 2, 2, DEF_PGA_CONF, SCHEDULE_CORE,
	LIST(`		', "PIPELINE_ID Capture Volume"))

# Mux has 2 sink and 2 source periods.
W_MUXDEMUX(0, 1, PIPELINE_FORMAT, 2, 2, SCHEDULE_CORE,
	LIST(`         ', concat(`DEMUX', PIPELINE_ID)))

# Capture Buffers
W_BUFFER(0, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS,
	COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)), PLATFORM_PASS_MEM_CAP)

W_BUFFER(1, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS,
	COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)), PLATFORM_PASS_MEM_CAP)

W_PIPELINE(SCHED_COMP, SCHEDULE_PERIOD, SCHEDULE_PRIORITY, SCHEDULE_CORE, SCHEDULE_TIME_DOMAIN, pipe_dai_schedule_plat)

#
# Pipeline Graph
#
#  demux <-- B0 <-- Volume 0 <-- B1

P_GRAPH(pipe-volume-demux, PIPELINE_ID,
	LIST(`		',
	`dapm(N_MUXDEMUX(0), N_BUFFER(0))',
	`dapm(N_BUFFER(0), N_PGA(0))',
	`dapm(N_PGA(0), N_BUFFER(1))'))
#
# Pipeline Source and Sinks
#
indir(`define', concat(`PIPELINE_SINK_', PIPELINE_ID), N_BUFFER(1))
indir(`define', concat(`PIPELINE_DEMUX_SOURCE_', PIPELINE_ID), N_MUXDEMUX(0))

undefine(`DEF_PGA_TOKENS')
undefine(`DEF_PGA_CONF')
