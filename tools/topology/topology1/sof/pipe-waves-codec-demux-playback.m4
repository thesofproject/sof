# Demux Volume Pipeline with Waves codec
#
#  Low Latency Playback with demux and volume.
#
# Pipeline Endpoints for connection are :-
#
#	Playback Demux
#	B3 (DAI buffer)
#
#
#  host PCM_P -- B0 --> Waves -> B1 --> volume -- B2 --> Demux(M) -- B3 --> sink DAI0
#                                                              |
#                                                          pipeline n+1 --> DAI
#

DECLARE_SOF_RT_UUID("Waves codec", waves_codec_uuid, 0xd944281a, 0xafe9,
		    0x4695, 0xa0, 0x43, 0xd7, 0xf6, 0x2b, 0x89, 0x53, 0x8e);
define(`CA_UUID', waves_codec_uuid)

# Include topology builder
include(`utils.m4')
include(`buffer.m4')
include(`pcm.m4')
include(`pga.m4')
include(`muxdemux.m4')
include(`mixercontrol.m4')
include(`bytecontrol.m4')
include(`dai.m4')
include(`pipeline.m4')
include(`codec_adapter.m4')

ifdef(`ENDPOINT_NAME',`',`fatal_error(`Pipe requires ENDPOINT_NAME to be defined: Speakers, Headphones, etc.')')

define(`SETUP_PARAMS_NAME', `Waves' `ENDPOINT_NAME' `Setup' PIPELINE_ID)

CONTROLBYTES_PRIV(PP_SETUP_CONFIG,
`       bytes "0x53,0x4f,0x46,0x00,'
`       0x00,0x00,0x00,0x00,'
`       0x0c,0x00,0x00,0x00,'
`       0x00,0x10,0x00,0x03,'
`       0x00,0x00,0x00,0x00,'
`       0x00,0x00,0x00,0x00,'
`       0x00,0x00,0x00,0x00,'
`       0x00,0x00,0x00,0x00,'
`       0x00,0x00,0x00,0x00,'
`       0x0c,0x00,0x00,0x00,'
`       0x00,0x00,0x00,0x00"'
)

# Post process Bytes control for setup config
C_CONTROLBYTES(SETUP_PARAMS_NAME, PIPELINE_ID,
        CONTROLBYTES_OPS(bytes),
        CONTROLBYTES_EXTOPS(void, 258, 258),
        , , ,
        CONTROLBYTES_MAX(, 8192),
        ,
        PP_SETUP_CONFIG)

# demux Bytes control with max value of 255
C_CONTROLBYTES(concat(`DEMUX', PIPELINE_ID), PIPELINE_ID,
	CONTROLBYTES_OPS(bytes, 258 binds the mixer control to bytes get/put handlers, 258, 258),
	CONTROLBYTES_EXTOPS(258 binds the mixer control to bytes get/put handlers, 258, 258),
	, , ,
	CONTROLBYTES_MAX(, 304),
	,	concat(`demux_priv_', PIPELINE_ID))

# Volume Mixer control with max value of 32
C_CONTROLMIXER(Master Playback Volume, PIPELINE_ID,
	CONTROLMIXER_OPS(volsw, 256 binds the mixer control to volume get/put handlers, 256, 256),
	CONTROLMIXER_MAX(, 32),
	false,
	CONTROLMIXER_TLV(TLV 32 steps from -64dB to 0dB for 2dB, vtlv_m64s2),
	Channel register and shift for Front Left/Right,
	VOLUME_CHANNEL_MAP)

#
# Volume configuration
#

define(DEF_PGA_TOKENS, concat(`pga_tokens_', PIPELINE_ID))
define(DEF_PGA_CONF, concat(`pga_conf_', PIPELINE_ID))

W_VENDORTUPLES(DEF_PGA_TOKENS, sof_volume_tokens,
LIST(`		', `SOF_TKN_VOLUME_RAMP_STEP_TYPE	"2"'
     `		', `SOF_TKN_VOLUME_RAMP_STEP_MS		"20"'))

W_DATA(DEF_PGA_CONF, DEF_PGA_TOKENS)

#
# Components and Buffers
#

# Host "Low latency Playback" PCM
# with 2 sink and 0 source periods
W_PCM_PLAYBACK(PCM_ID, Low Latency Playback, 2, 0, SCHEDULE_CORE)

W_CODEC_ADAPTER(0, PIPELINE_FORMAT, DAI_PERIODS, DAI_PERIODS, SCHEDULE_CORE,
        LIST(`          ', "SETUP_PARAMS_NAME"))

# "Master Playback Volume" has 2 source and x sink periods for DAI ping-pong
W_PGA(1, PIPELINE_FORMAT, DAI_PERIODS, 2, DEF_PGA_CONF, SCHEDULE_CORE,
	LIST(`		', "PIPELINE_ID Master Playback Volume"))

# Mux 0 has 2 sink and source periods.
W_MUXDEMUX(0, 1, PIPELINE_FORMAT, 2, 2, SCHEDULE_CORE,
	LIST(`         ', concat(`DEMUX', PIPELINE_ID)))

# Low Latency Buffers
W_BUFFER(0, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_HOST_MEM_CAP)
W_BUFFER(1, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_COMP_MEM_CAP)
W_BUFFER(2, COMP_BUFFER_SIZE(DAI_PERIODS,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_COMP_MEM_CAP)
W_BUFFER(3, COMP_BUFFER_SIZE(DAI_PERIODS,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_COMP_MEM_CAP)

#
# Pipeline Graph
#
#  host PCM_P --B0--> Waves --B1--> Demux --B2--> volume ---B3--> sink DAI0

P_GRAPH(pipe-waves-codec-demux-playback, PIPELINE_ID,
	LIST(`		',
	`dapm(N_BUFFER(0), N_PCMP(PCM_ID))',
	`dapm(N_CODEC_ADAPTER(0), N_BUFFER(0))',
	`dapm(N_BUFFER(1), N_CODEC_ADAPTER(0))',
	`dapm(N_PGA(1), N_BUFFER(1))',
	`dapm(N_BUFFER(2), N_PGA(1))',
	`dapm(N_MUXDEMUX(0), N_BUFFER(2))',
	`dapm(N_BUFFER(3), N_MUXDEMUX(0))'))
#
# Pipeline Source and Sinks
#
indir(`define', concat(`PIPELINE_SOURCE_', PIPELINE_ID), N_BUFFER(3))
indir(`define', concat(`PIPELINE_DEMUX_', PIPELINE_ID), N_MUXDEMUX(0))
indir(`define', concat(`PIPELINE_PCM_', PIPELINE_ID), Low Latency Playback PCM_ID)

#
# PCM Configuration
#


# PCM capabilities supported by FW
PCM_CAPABILITIES(Low Latency Playback PCM_ID, CAPABILITY_FORMAT_NAME(PIPELINE_FORMAT), 48000, 48000, 2, PIPELINE_CHANNELS, 2, 16, 192, 16384, 65536, 65536)

undefine(`DEF_PGA_TOKENS')
undefine(`DEF_PGA_CONF')
