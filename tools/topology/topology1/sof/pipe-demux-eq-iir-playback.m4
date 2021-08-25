# Demux EQ band split pipeline
#
# host PCM_P --B0--> demux(M) --B1--> eq_iir --B2--> sink DAI0
#

# Include topology builder
include(`utils.m4')
include(`buffer.m4')
include(`pcm.m4')
include(`muxdemux.m4')
include(`bytecontrol.m4')
include(`eq_iir.m4')

dnl Configure demux
dnl name, pipeline_id, routing_matrix_rows
dnl Diagonal 1's in routing matrix mean that every input channel is
dnl copied to corresponding output channels in all output streams.
dnl I.e. row index is the input channel, 1 means it is copied to
dnl corresponding output channel (column index), 0 means it is discarded.
dnl There's a separate matrix for all outputs.
define(matrix1, `ROUTE_MATRIX(1,
			     `BITS_TO_BYTE(1, 0, 0 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(1, 0, 0 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 1, 0 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 1, 0 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,0 ,0 ,0)')')

dnl name, num_streams, route_matrix list
MUXDEMUX_CONFIG(demux_priv_1, 1, LIST(`	', `matrix1'))

# demux Bytes control with max value of 255
C_CONTROLBYTES(concat(`DEMUX', PIPELINE_ID), PIPELINE_ID,
	CONTROLBYTES_OPS(bytes, 258 binds the mixer control to bytes get/put handlers, 258, 258),
	CONTROLBYTES_EXTOPS(258 binds the mixer control to bytes get/put handlers, 258, 258),
	, , ,
	CONTROLBYTES_MAX(, 304),
	,	concat(`demux_priv_', PIPELINE_ID))

# EQ IIR Bytes control
define(DEF_EQIIR_COEF, concat(`eqiir_coef_', PIPELINE_ID))
define(DEF_EQIIR_PRIV, concat(`eqiir_priv_', PIPELINE_ID))

# Bandsplit, created with example_iir_bandsplit.m 25-Aug-2021
CONTROLBYTES_PRIV(DEF_EQIIR_PRIV,
`       bytes "0x53,0x4f,0x46,0x00,0x00,0x00,0x00,0x00,'
`       0xb0,0x00,0x00,0x00,0x00,0x30,0x01,0x03,'
`       0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'
`       0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'
`       0xb0,0x00,0x00,0x00,0x04,0x00,0x00,0x00,'
`       0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'
`       0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'
`       0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'
`       0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'
`       0x01,0x00,0x00,0x00,0x02,0x00,0x00,0x00,'
`       0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'
`       0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'
`       0x00,0x00,0x00,0x00,0x50,0xda,0xf0,0xc0,'
`       0x1e,0x5d,0x0d,0x7f,0x3c,0xdf,0xd6,0x1f,'
`       0x89,0x41,0x52,0xc0,0x3c,0xdf,0xd6,0x1f,'
`       0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x00,'
`       0x2d,0x3a,0xcd,0xd3,0xc0,0xf5,0x82,0x68,'
`       0xb9,0x41,0x76,0x00,0x72,0x83,0xec,0x00,'
`       0xb9,0x41,0x76,0x00,0xff,0xff,0xff,0xff,'
`       0x99,0x7f,0x00,0x00,0x01,0x00,0x00,0x00,'
`       0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'
`       0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'
`       0x00,0x00,0x00,0x00,0x2d,0x3a,0xcd,0xd3,'
`       0xc0,0xf5,0x82,0x68,0x01,0xe1,0xa6,0x1a,'
`       0xff,0x3d,0xb2,0xca,0x01,0xe1,0xa6,0x1a,'
`       0x00,0x00,0x00,0x00,0xb2,0x7f,0x00,0x00"'
)

C_CONTROLBYTES(DEF_EQIIR_COEF, PIPELINE_ID,
	CONTROLBYTES_OPS(bytes, 258 binds the control to bytes get/put handlers, 258, 258),
	CONTROLBYTES_EXTOPS(258 binds the control to bytes get/put handlers, 258, 258),
	, , ,
	CONTROLBYTES_MAX(, 1024),
	,
	DEF_EQIIR_PRIV)

#
# Components and Buffers
#

# Host "Speaker Playback" PCM
# with 2 sink and 0 source periods
W_PCM_PLAYBACK(PCM_ID, Speaker Playback, 2, 0, SCHEDULE_CORE)

# "EQ IIR" has x sink period and 2 source periods
W_EQ_IIR(0, PIPELINE_FORMAT, 2, 2, SCHEDULE_CORE,
	LIST(`		', "DEF_EQIIR_COEF"))

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
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), 4, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_COMP_MEM_CAP)

#
# Pipeline Graph
#
#  host PCM_P --B0--> Demux(M) --B1--> eq_iir --B2--> sink DAI0

P_GRAPH(pipe-demux-eq-playback, PIPELINE_ID,
	LIST(`		',
	`dapm(N_BUFFER(0), N_PCMP(PCM_ID))',
	`dapm(N_MUXDEMUX(0), N_BUFFER(0))',
	`dapm(N_BUFFER(1), N_MUXDEMUX(0))',
	`dapm(N_EQ_IIR(0), N_BUFFER(1))',
	`dapm(N_BUFFER(2), N_EQ_IIR(0))'))

#
# Pipeline Source and Sinks
#
indir(`define', concat(`PIPELINE_SOURCE_', PIPELINE_ID), N_BUFFER(2))
indir(`define', concat(`PIPELINE_PCM_', PIPELINE_ID), Speaker Playback PCM_ID)

#
# PCM Configuration
#


# PCM capabilities supported by FW
PCM_CAPABILITIES(Speaker Playback PCM_ID, CAPABILITY_FORMAT_NAME(PIPELINE_FORMAT), 48000, 48000, 2, PIPELINE_CHANNELS, 2, 16, 192, 16384, 65536, 65536)

undefine(`matrix1')
undefine(`DEF_EQIIR_COEF')
undefine(`DEF_EQIIR_PRIV')
