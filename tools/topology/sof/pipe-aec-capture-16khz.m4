# Capture AEC Pipeline and PCM
#
# Pipeline Endpoints for connection are :-
#
#  host PCM_C <--- B1 <--- AEC 0 <-- B0 <-- sink DAI0
#                           ^
#                           |
#                           +--- B2 <--- SRC 0 <--- B3 <AEC_REFSNK_48K>

#
# PCM 48k ---> volume ---> demux ---> DAI 48k
#                            |
#                            v
#                           SRC
#                            |
#                            v
# PCM 16k <---------------- AEC <--- DAI 16k
#

# Include topology builder
include(`utils.m4')
include(`buffer.m4')
include(`pcm.m4')
include(`dai.m4')
include(`pipeline.m4')
include(`bytecontrol.m4')
include(`aec.m4')
include(`src.m4')

#
# Controls
#

# Just some random data (from IIR)

define(DEF_AEC_PRIV, concat(`aec_priv_', PIPELINE_ID))
define(DEF_AEC_BYTES, concat(`aec_bytes_', PIPELINE_ID))

CONTROLBYTES_PRIV(DEF_AEC_PRIV,
`       bytes "0x53,0x4f,0x46,0x00,0x00,0x00,0x00,0x00,'
`       0x58,0x00,0x00,0x00,0x00,0x00,0x00,0x03,'
`       0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'
`       0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'
`       0x58,0x00,0x00,0x00,0x02,0x00,0x00,0x00,'
`       0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'
`       0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'
`       0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'
`       0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,'
`       0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'
`       0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'
`       0x00,0x00,0x00,0x00,0x63,0xf3,0x96,0xc0,'
`       0xc6,0x59,0x68,0x7f,0x6d,0x89,0xed,0x1f,'
`       0x27,0xed,0x24,0xc0,0x6d,0x89,0xed,0x1f,'
`       0xfc,0xff,0xff,0xff,0xd0,0x4f,0x00,0x00"'
)

# AEC Bytes control with max value of 255
# AEC configuration data is 642 bytes plus SOF headers
C_CONTROLBYTES(DEF_AEC_BYTES, PIPELINE_ID,
	CONTROLBYTES_OPS(bytes, 258 binds the mixer control to bytes get/put handlers, 258, 258),
	CONTROLBYTES_EXTOPS(258 binds the mixer control to bytes get/put handlers, 258, 258),
	, , ,
	CONTROLBYTES_MAX(, 400),
	,
	DEF_AEC_PRIV)

#
# SRC Configuration
#

define(DEF_SRC_TOKENS, concat(`src_tokens_', PIPELINE_ID))
define(DEF_SRC_CONF, concat(`src_conf_', PIPELINE_ID))
W_VENDORTUPLES(DEF_SRC_TOKENS, sof_src_tokens,
	LIST(`		', `SOF_TKN_SRC_RATE_IN		"48000"'
	     `		', `SOF_TKN_SRC_RATE_OUT	"16000"'))
W_DATA(DEF_SRC_CONF, DEF_SRC_TOKENS)

#
# Components and Buffers
#

# Host "AEC Capture" PCM
# with 0 sink and 2 source periods
W_PCM_CAPTURE(PCM_ID, AEC Capture, 0, 2, SCHEDULE_CORE)

# "AEC 0" has 2 sink period and 2 source periods
W_AEC(0, PIPELINE_FORMAT, 2, DAI_PERIODS, SCHEDULE_CORE, LIST(`		', "DEF_AEC_BYTES"))

# "SRC" has 2 source and 2 sink periods
W_SRC(0, PIPELINE_FORMAT, 2, 2, DEF_SRC_CONF, SCHEDULE_CORE)

# Capture Buffers
W_BUFFER(0, COMP_BUFFER_SIZE(DAI_PERIODS,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS,
	eval(PCM_MAX_RATE / SCHEDULE_PERIOD)), PLATFORM_PASS_MEM_CAP)

W_BUFFER(1, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS,
	eval(PCM_MAX_RATE / SCHEDULE_PERIOD)), PLATFORM_PASS_MEM_CAP)

W_BUFFER(2, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS,
	eval(PCM_MAX_RATE / SCHEDULE_PERIOD)), PLATFORM_PASS_MEM_CAP)

W_BUFFER(3, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS,
	eval(PCM_MAX_RATE / SCHEDULE_PERIOD)), PLATFORM_PASS_MEM_CAP)

#
# Pipeline Graph
#
#  host PCM_C <--- B1 <--- AEC 0 <-- B0 <-- sink DAI0
#                           ^
#                           |
#                           +--- B2 <--- SRC 0 <--- B3 <AEC_REFSNK_48K>

P_GRAPH(pipe-aec-capture-PIPELINE_ID, PIPELINE_ID,
	LIST(`		',
	`dapm(N_PCMC(PCM_ID), N_BUFFER(1))',
	`dapm(N_BUFFER(1), N_AEC(0))',
        `dapm(N_AEC(0), N_BUFFER(0))',
	`dapm(N_AEC(0), N_BUFFER(2))',
	`dapm(N_BUFFER(2), N_SRC(0))',
	`dapm(N_SRC(0), N_BUFFER(3))'))

#
# Pipeline Source and Sinks
#
indir(`define', PIPELINE_AEC_REFSNK_ID, PIPELINE_ID)
indir(`define', PIPELINE_AEC_REFSNK_48K, N_BUFFER(3))
indir(`define', concat(`PIPELINE_SINK_', PIPELINE_ID), N_BUFFER(0))
indir(`define', concat(`PIPELINE_PCM_', PIPELINE_ID), AEC Capture PCM_ID)

#
# PCM Configuration
#

PCM_CAPABILITIES(AEC Capture PCM_ID, CAPABILITY_FORMAT_NAME(PIPELINE_FORMAT),
	PCM_MIN_RATE, PCM_MAX_RATE, PIPELINE_CHANNELS, PIPELINE_CHANNELS,
	2, 16, 192, 16384, 65536, 65536)

undefine(`DEF_SRC_TOKENS')
undefine(`DEF_SRC_CONF')
undefine(`DEF_AEC_PRIV')
undefine(`DEF_AEC_BYTES')
