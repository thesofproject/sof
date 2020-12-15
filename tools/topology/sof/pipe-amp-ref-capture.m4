# Amplifier feedback reference Demux Pipeline
#
#  capture with demux for both echo reference to user space and the feedback to smart amplifier FW algorithm.
#
# Pipeline Endpoints for connection are :-
#
#	Capture Demux
#	B1 (DAI buffer)
#
#
#                          |
#  host PCM_C <-- B0 <-- Demux(M) <-- B1 <-- DAI
#

# Include topology builder
include(`utils.m4')
include(`buffer.m4')
include(`pcm.m4')
include(`pga.m4')
include(`muxdemux.m4')
include(`mixercontrol.m4')
include(`bytecontrol.m4')

ifdef(`SMART_TX_CHANNELS',`',`errprint(note: Need to define DAI TX channel number for sof-smart-amplifier
)')
ifdef(`SMART_RX_CHANNELS',`',`errprint(note: Need to define DAI RX channel number for sof-smart-amplifier
)')
ifdef(`SMART_FB_CHANNELS',`',`errprint(note: Need to define feedback channel number for sof-smart-amplifier
)')

ifdef(`SMART_PB_PPL_ID',`',`errprint(note: Need to define playback pipeline ID for sof-smart-amplifier
)')
ifdef(`SMART_PB_CH_NUM',`',`errprint(note: Need to define playback channel number for sof-smart-amplifier
)')

ifdef(`SMART_REF_PPL_ID',`',`errprint(note: Need to define Echo Ref pipeline ID for sof-smart-amplifier
)')
ifdef(`SMART_REF_CH_NUM',`',`errprint(note: Need to define Echo Ref channel number for sof-smart-amplifier
)')

ifelse(SMART_FB_CHANNELS, `8',
`define(`FB_CHMAP',`0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80')',
`define(`FB_CHMAP',`0x01,0x02,0x04,0x08,0x00,0x00,0x00,0x00')'
)

ifelse(SMART_REF_CH_NUM, `4',
`define(`REF_CHMAP',`0x01,0x02,0x04,0x08,0x00,0x00,0x00,0x00')',
`define(`REF_CHMAP',`0x01,0x04,0x00,0x00,0x00,0x00,0x00,0x00')'
)


# should be aligned with struct sof_mux_config, used for mux input/output configuration.
CONTROLBYTES_PRIV(DEMUX_priv,
`       bytes "0x53,0x4f,0x46,0x00,0x00,0x00,0x00,0x00,'
`       0x28,0x00,0x00,0x00,0x00,0x60,0x00,0x03,'
`       0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'
`       0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'
`       COMP_FORMAT_VALUE(PIPELINE_FORMAT),0x00,DEC2HEX(SMART_RX_CHANNELS),0x00,0x02,0x00,0x00,0x00,'
`       DEC2HEX(SMART_PB_PPL_ID),0x00,0x00,0x00,DEC2HEX(SMART_FB_CHANNELS),FB_CHMAP,0x00,0x00,0x00,'
`       DEC2HEX(SMART_REF_PPL_ID),0x00,0x00,0x00,DEC2HEX(SMART_REF_CH_NUM),REF_CHMAP,0x00,0x00,0x00"'
)

# demux Bytes control with max value of 255
C_CONTROLBYTES(DEMUX, PIPELINE_ID,
	CONTROLBYTES_OPS(bytes, 258 binds the mixer control to bytes get/put handlers, 258, 258),
	CONTROLBYTES_EXTOPS(258 binds the mixer control to bytes get/put handlers, 258, 258),
	, , ,
	CONTROLBYTES_MAX(, 304),
	,
	DEMUX_priv)
#
# Components and Buffers
#

# Host "Echo Reference Capture" PCM
# with 0 sink and 2 source periods
W_PCM_CAPTURE(PCM_ID, Echo Reference Capture, 0, 2, SCHEDULE_CORE)

# Mux 0 has 2 sink and source periods.
W_MUXDEMUX(0, 1, PIPELINE_FORMAT, 2, 2, SCHEDULE_CORE, LIST(`         ', "DEMUX"))

# define demux widget name for up layer connection
define(`N_SMART_DEMUX',N_MUXDEMUX(0))

# Low Latency Buffers
W_BUFFER(0, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_HOST_MEM_CAP)
W_BUFFER(1, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), SMART_RX_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_COMP_MEM_CAP)

#
# Pipeline Graph
#
#  host PCM_C <--B0-- Mux(M) <--B1-- sink DAI0
#

P_GRAPH(pipe-demux-capture-PIPELINE_ID, PIPELINE_ID,
	LIST(`		',
	`dapm(N_PCMC(PCM_ID), N_BUFFER(0))',
	`dapm(N_BUFFER(0), N_MUXDEMUX(0))',
	`dapm(N_MUXDEMUX(0), N_BUFFER(1))'))

#
# Pipeline Source and Sinks
#
indir(`define', concat(`PIPELINE_SINK_', PIPELINE_ID), N_BUFFER(1))
indir(`define', concat(`PIPELINE_DEMUX_', PIPELINE_ID), N_MUXDEMUX(0))
indir(`define', concat(`PIPELINE_PCM_', PIPELINE_ID), Echo Reference Capture PCM_ID)

#
# PCM Configuration
#


# PCM capabilities supported by FW
PCM_CAPABILITIES(Echo Reference Capture PCM_ID,
	CAPABILITY_FORMAT_NAME(PIPELINE_FORMAT),
	48000, 48000, dnl rate_min, rate_max
	SMART_REF_CH_NUM, SMART_REF_CH_NUM, dnl channels_min, channels_max
	2, 16, dnl periods_min, periods_max
	192, 16384, dnl period_size_min, period_size_max
	65536, 65536)

