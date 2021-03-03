# Demux Volume Pipeline
#
#  Low Latency Playback with volume, demux, src
#
#  host PCM_P -- B0 --> volume --> B1 --> Demux(M) --> B2 --> sink DAI0
#                                         |
#                                         v

# Include topology builder
include(`utils.m4')
include(`buffer.m4')
include(`pcm.m4')
include(`pga.m4')
include(`muxdemux.m4')
include(`mixercontrol.m4')
include(`bytecontrol.m4')
include(`bytecontrol.m4')

define(matrix1, `ROUTE_MATRIX(PIPELINE_ID,
                             `BITS_TO_BYTE(1, 0, 0 ,0 ,0 ,0 ,0 ,0)',
                             `BITS_TO_BYTE(0, 1, 0 ,0 ,0 ,0 ,0 ,0)',
                             `BITS_TO_BYTE(0, 0, 1 ,0 ,0 ,0 ,0 ,0)',
                             `BITS_TO_BYTE(0, 0, 0 ,1 ,0 ,0 ,0 ,0)',
                             `BITS_TO_BYTE(0, 0, 0 ,0 ,1 ,0 ,0 ,0)',
                             `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,1 ,0 ,0)',
                             `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,0 ,1 ,0)',
                             `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,0 ,0 ,1)')')

define(matrix2, `ROUTE_MATRIX(PIPELINE_AEC_REFSNK_ID,
                             `BITS_TO_BYTE(1, 0, 0 ,0 ,0 ,0 ,0 ,0)',
                             `BITS_TO_BYTE(0, 1, 0 ,0 ,0 ,0 ,0 ,0)',
                             `BITS_TO_BYTE(0, 0, 1 ,0 ,0 ,0 ,0 ,0)',
                             `BITS_TO_BYTE(0, 0, 0 ,1 ,0 ,0 ,0 ,0)',
                             `BITS_TO_BYTE(0, 0, 0 ,0 ,1 ,0 ,0 ,0)',
                             `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,1 ,0 ,0)',
                             `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,0 ,1 ,0)',
                             `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,0 ,0 ,1)')')

MUXDEMUX_CONFIG(concat(`demux_priv_', PIPELINE_ID), 2, LIST(`	', `matrix1,', `matrix2'))

# demux Bytes control with max value of 255
C_CONTROLBYTES(concat(`DEMUX', PIPELINE_ID), PIPELINE_ID,
	CONTROLBYTES_OPS(bytes, 258 binds the mixer control to bytes get/put handlers, 258, 258),
	CONTROLBYTES_EXTOPS(258 binds the mixer control to bytes get/put handlers, 258, 258),
	, , ,
	CONTROLBYTES_MAX(, 304),
	,
	concat(`demux_priv_', PIPELINE_ID))

# Volume Mixer control with max value of 32
C_CONTROLMIXER(Master Playback Volume, PIPELINE_ID,
	CONTROLMIXER_OPS(volsw, 256 binds the mixer control to volume get/put handlers, 256, 256),
	CONTROLMIXER_MAX(, 32),
	false,
	CONTROLMIXER_TLV(TLV 32 steps from -64dB to 0dB for 2dB, vtlv_m64s2),
	Channel register and shift for Front Left/Right,
	LIST(`	', KCONTROL_CHANNEL(FL, 1, 0), KCONTROL_CHANNEL(FR, 1, 1)))

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

# "Master Playback Volume" has 2 source and 2 sink periods for DAI ping-pong
W_PGA(0, PIPELINE_FORMAT, 2, 2, DEF_PGA_CONF, SCHEDULE_CORE,
	 LIST(`		', "PIPELINE_ID Master Playback Volume"))

# Mux 0 has 2 sink and source periods.
W_MUXDEMUX(0, 1, PIPELINE_FORMAT, DAI_PERIODS, 2, SCHEDULE_CORE,
	      LIST(`         ', concat(`DEMUX', PIPELINE_ID)))

# Low Latency Buffers
W_BUFFER(0, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS,
	COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_HOST_MEM_CAP)
W_BUFFER(1, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS,
	COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_COMP_MEM_CAP)
W_BUFFER(2, COMP_BUFFER_SIZE(DAI_PERIODS,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS,
	COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_COMP_MEM_CAP)

#
# Pipeline Graph
#
#  host PCM_P -- B0 --> volume --> B1 --> Demux(M) --> B2 --> sink DAI0
#                                           |
#                                           v
#                                    <AEC_REFSRC_48K>
#

P_GRAPH(pipe-ll-playback-PIPELINE_ID, PIPELINE_ID,
	LIST(`		',
	`dapm(N_BUFFER(0), N_PCMP(PCM_ID))',
	`dapm(N_PGA(0), N_BUFFER(0))',
	`dapm(N_BUFFER(1), N_PGA(0))',
	`dapm(N_MUXDEMUX(0), N_BUFFER(1))',
	`dapm(N_BUFFER(2), N_MUXDEMUX(0))'))

#
# Pipeline Source and Sinks
#
indir(`define', concat(`PIPELINE_SOURCE_', PIPELINE_ID), N_BUFFER(2))
indir(`define', concat(`PIPELINE_PCM_', PIPELINE_ID), Low Latency Playback PCM_ID)
indir(`define', PIPELINE_AEC_REFSRC_48K, N_MUXDEMUX(0))

#
# PCM Configuration
#

# PCM capabilities supported by FW
PCM_CAPABILITIES(Low Latency Playback PCM_ID, CAPABILITY_FORMAT_NAME(PIPELINE_FORMAT), 48000, 48000, 2, PIPELINE_CHANNELS, 2, 16, 192, 16384, 65536, 65536)

undefine(`matrix1')
undefine(`matrix2')
undefine(`DEF_PGA_TOKENS')
undefine(`DEF_PGA_CONF')

#
# Virtual DAI for reference path
#

#SectionGraph."PIPE_REF_CONNECT" {
#        index "1000"
#
#        lines [
#               # Todo, how to pass DAI name to here
#		dapm(ECHO REF PIPELINE_AEC_REFSNK_ID, DMIC1.IN)
#		dapm(ECHO REF PIPELINE_AEC_REFSNK_ID, DMIC0.IN)
#        ]
#}

#
# Finally connect AEC reference path
#

SectionGraph."PIPE_REF_VIRTUAL" {
        index "1001"

        lines [
		dapm(PIPELINE_AEC_REFSNK_48K, PIPELINE_AEC_REFSRC_48K)
        ]
}
