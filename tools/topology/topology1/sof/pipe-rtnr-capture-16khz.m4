# Capture RTNR Pipeline and PCM
#
# Pipeline Endpoints for connection are :-
#
#  host PCM_C <--- B1 <--- RTNR 0 <-- B0 <-- sink DAI0
#
#
#

# Include topology builder
include(`utils.m4')
include(`buffer.m4')
include(`pcm.m4')
include(`dai.m4')
include(`pipeline.m4')
include(`mixercontrol.m4')
include(`bytecontrol.m4')
include(`rtnr.m4')

ifdef(`RTNR_BUFFER_SIZE_MIN',`', define(RTNR_BUFFER_SIZE_MIN, `65536'))
ifdef(`RTNR_BUFFER_SIZE_MAX',`', define(RTNR_BUFFER_SIZE_MAX, `65536'))
#
# Controls
#

define(DEF_RTNR_PRIV, concat(`rtnr_priv_', PIPELINE_ID))
define(DEF_RTNR_BYTES, concat(`rtnr_bytes_', PIPELINE_ID))

CONTROLBYTES_PRIV(DEF_RTNR_PRIV,
`       bytes "0x53,0x4f,0x46,0x00,0x00,0x00,0x00,0x00,'
`       0x20,0x00,0x00,0x00,0x00,0x30,0x01,0x03,'
`       0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'
`       0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'
`       0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'
`       0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'
`       0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,'
`       0x80,0x3e,0x00,0x00,0x00,0x00,0x00,0x00"'
)

# RTNR Bytes control with max value of 255
C_CONTROLBYTES(DEF_RTNR_BYTES, PIPELINE_ID,
	CONTROLBYTES_OPS(bytes, 258 binds the mixer control to bytes get/put handlers, 258, 258),
	CONTROLBYTES_EXTOPS(258 binds the mixer control to bytes get/put handlers, 258, 258),
	, , ,
	CONTROLBYTES_MAX(, 256),
	,
	DEF_RTNR_PRIV)

# RTNR Enable switch
define(DEF_RTNR_ENABLE, concat(`rtnr_enable_', PIPELINE_ID))
define(`CONTROL_NAME', `DEF_RTNR_ENABLE')

C_CONTROLMIXER(DEF_RTNR_ENABLE, PIPELINE_ID,
	CONTROLMIXER_OPS(volsw, 259 binds the mixer control to switch get/put handlers, 259, 259),
	CONTROLMIXER_MAX(max 1 indicates switch type control, 1),
	false,
	,
	Channel register and shift for Front Center,
	LIST(`	', KCONTROL_CHANNEL(FC, 3, 0)),
	"1")
undefine(`CONTROL_NAME')

#
# Components and Buffers
#

# Host "Capture" PCM
# with 0 sink and 2 source periods
W_PCM_CAPTURE(PCM_ID, Capture, 0, 2, DMIC_PIPELINE_16k_CORE_ID)

# "RTNR 0" has 2 sink period and 2 source periods
W_RTNR(0, PIPELINE_FORMAT, 2, DAI_PERIODS, SCHEDULE_CORE,
	LIST(`		', "DEF_RTNR_BYTES"),
	LIST(`          ', "DEF_RTNR_ENABLE"))

# Capture Buffers
W_BUFFER(0, COMP_BUFFER_SIZE(4,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS,
	COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)), PLATFORM_PASS_MEM_CAP)

W_BUFFER(1, COMP_BUFFER_SIZE(4,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS,
	COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)), PLATFORM_PASS_MEM_CAP)

P_GRAPH(pipe-rtnr-capture-PIPELINE_ID, PIPELINE_ID,
	LIST(`		',
	`dapm(N_PCMC(PCM_ID), N_BUFFER(1))',
	`dapm(N_BUFFER(1), N_RTNR(0))',
        `dapm(N_RTNR(0), N_BUFFER(0))'))

#
# Pipeline Source and Sinks
#
indir(`define', concat(`PIPELINE_SINK_', PIPELINE_ID), N_BUFFER(0))
indir(`define', concat(`PIPELINE_PCM_', PIPELINE_ID), Capture PCM_ID)

#
# PCM Configuration
#

PCM_CAPABILITIES(Capture PCM_ID, CAPABILITY_FORMAT_NAME(PIPELINE_FORMAT),
	PCM_MIN_RATE, PCM_MAX_RATE, PIPELINE_CHANNELS, PIPELINE_CHANNELS,
	2, 16, 192, 16384, RTNR_BUFFER_SIZE_MIN, RTNR_BUFFER_SIZE_MAX)

undefine(`DEF_RTNR_ENABLE')
undefine(`DEF_RTNR_PRIV')
undefine(`DEF_RTNR_BYTES')
