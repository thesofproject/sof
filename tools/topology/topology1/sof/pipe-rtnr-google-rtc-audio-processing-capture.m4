# Acoustic echo cancelling Pipeline and PCM
#
# Pipeline Endpoints for connection are :-
#
#  host PCM_C <-- B0 <- NC <-- B1 <-- AEC <-- B2 <-- source DAI0
#                                      ^----- B3 <-- AEC reference

# Include topology builder
include(`utils.m4')
include(`buffer.m4')
include(`pcm.m4')
include(`dai.m4')
include(`mixercontrol.m4')
include(`pipeline.m4')
include(`bytecontrol.m4')
include(`enumcontrol.m4')
include(`google_rtc_audio_processing.m4')
include(`rtnr.m4')

define(GOOGLE_RTC_AUDIO_PROCESSING_priv, concat(`google_rtc_audio_processing_bytes_', PIPELINE_ID))
define(GOOGLE_RTC_AUDIO_PROCESSING_CTRL, concat(`google_rtc_audio_processing_control_', PIPELINE_ID))

include(`google_rtc_audio_processing_default.m4')

define(DEF_RTNR_PRIV, concat(`rtnr_priv_', PIPELINE_ID))
define(DEF_RTNR_BYTES, concat(`rtnr_bytes_', PIPELINE_ID))
define(DEF_RTNR_DATA, concat(`rtnr_data', PIPELINE_ID))
define(DEF_RTNR_DATA_BYTES, concat(`rtnr_data_', PIPELINE_ID))

ifdef(`RTNR_BUFFER_SIZE_MIN',`', define(RTNR_BUFFER_SIZE_MIN, `65536'))
ifdef(`RTNR_BUFFER_SIZE_MAX',`', define(RTNR_BUFFER_SIZE_MAX, `65536'))

CONTROLBYTES_PRIV(DEF_RTNR_PRIV,
`       bytes "0x53,0x4f,0x46,0x00,0x00,0x00,0x00,0x00,'
`       0x20,0x00,0x00,0x00,0x00,0x30,0x01,0x03,'
`       0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'
`       0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'
`       0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'
`       0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'
`       0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,'
`       0x80,0xbb,0x00,0x00,0x00,0x00,0x00,0x00"'
)

# RTNR Bytes control with max value of 255
C_CONTROLBYTES_READONLY(DEF_RTNR_BYTES, PIPELINE_ID,
        CONTROLBYTES_OPS(bytes, 258 binds the mixer control to bytes get/put handlers, 258),
        CONTROLBYTES_EXTOPS(258 binds the mixer control to bytes get/put handlers, 258),
        , , ,
        CONTROLBYTES_MAX(, 256),
        ,
        DEF_RTNR_PRIV)

CONTROLBYTES_PRIV(DEF_RTNR_DATA,
`       bytes "0x53,0x4f,0x46,0x00,0x01,0x00,0x00,0x00,'
`       0x00,0x00,0x00,0x00,0x00,0x30,0x01,0x03,'
`       0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'
`       0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00"'
)

# Bytes control for RTNR Data blob
C_CONTROLBYTES(DEF_RTNR_DATA_BYTES, PIPELINE_ID,
        CONTROLBYTES_OPS(bytes, 258 binds the mixer control to bytes get/put handlers, 258, 258),
        CONTROLBYTES_EXTOPS(258 binds the mixer control to bytes get/put handlers, 258, 258),
        , , ,
        CONTROLBYTES_MAX(, 10240),
        ,
        DEF_RTNR_DATA)

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

C_CONTROLBYTES(`Config', PIPELINE_ID,
	CONTROLBYTES_OPS(bytes, 258 binds the control to bytes get/put handlers, 258, 258),
	CONTROLBYTES_EXTOPS(258 binds the control to bytes get/put handlers, 258, 258),
	, , ,
	CONTROLBYTES_MAX(, 2048),
	,
	GOOGLE_RTC_AUDIO_PROCESSING_priv)

#
# Components and Buffers
#

# Host "Google RTC Audio Processing Capture" PCM
# with 0 sink and 2 source periods
W_PCM_CAPTURE(PCM_ID, Google RTC Audio Processing, 0, DAI_PERIODS, SCHEDULE_CORE)

W_GOOGLE_RTC_AUDIO_PROCESSING(0, PIPELINE_FORMAT, 2, DAI_PERIODS, SCHEDULE_CORE,
			`',
			LIST(`          ', "Config"))

# "RTNR 0" has 2 sink period and 2 source periods
W_RTNR(0, PIPELINE_FORMAT, 2, DAI_PERIODS, SCHEDULE_CORE,
    LIST(`		', "DEF_RTNR_BYTES", "DEF_RTNR_DATA_BYTES"),
    LIST(`          ', "DEF_RTNR_ENABLE"))

# Capture Buffers
W_BUFFER(0, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_HOST_MEM_CAP)
W_BUFFER(1, COMP_BUFFER_SIZE(DAI_PERIODS,
	COMP_SAMPLE_SIZE(DAI_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_DAI_MEM_CAP)
W_BUFFER(2, COMP_BUFFER_SIZE(DAI_PERIODS,
	COMP_SAMPLE_SIZE(DAI_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_DAI_MEM_CAP)
W_BUFFER(3, COMP_BUFFER_SIZE(DAI_PERIODS,
	COMP_SAMPLE_SIZE(DAI_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_DAI_MEM_CAP)

define(`N_AEC_REF_BUF',`BUF'PIPELINE_ID`.'3)
#
# Pipeline Graph
#
#  host PCM_P <-- B0 <-- RTNR0 <-- B1 <-- AEC0 <-- B2 <-- sink DAI0
#                                          ^------- B3 <-- AEC ref

P_GRAPH(pipe-google-aec-capture-PIPELINE_ID, PIPELINE_ID,
	LIST(`		',
	`dapm(N_PCMC(PCM_ID), N_BUFFER(0))',
	`dapm(N_BUFFER(0), N_RTNR(0))',
	`dapm(N_RTNR(0), N_BUFFER(1))',
	`dapm(N_BUFFER(1), N_GOOGLE_RTC_AUDIO_PROCESSING(0))',
	`dapm(N_GOOGLE_RTC_AUDIO_PROCESSING(0), N_BUFFER(2))',
	`dapm(N_GOOGLE_RTC_AUDIO_PROCESSING(0), N_BUFFER(3))'))

#
# Pipeline Source and Sinks
#
indir(`define', concat(`PIPELINE_SINK_', PIPELINE_ID), N_BUFFER(2))
indir(`define', concat(`PIPELINE_PCM_', PIPELINE_ID), Google RTC Audio Processing PCM_ID)

#
# PCM Configuration
#

PCM_CAPABILITIES(Google RTC Audio Processing PCM_ID, CAPABILITY_FORMAT_NAME(PIPELINE_FORMAT),
	PCM_MIN_RATE, PCM_MAX_RATE, PIPELINE_CHANNELS, PIPELINE_CHANNELS,
	2, 16, 192, 16384, 65536, 65536)

undefine(`DEF_RTNR_ENABLE')
undefine(`DEF_RTNR_PRIV')
undefine(`DEF_RTNR_BYTES')
undefine(`DEF_RTNR_DATA')
undefine(`DEF_RTNR_DATA_BYTES')
