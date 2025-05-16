# CTC Pipeline and PCM
#
# Pipeline Endpoints for connection are :-
#
# host PCM_P --> B0 --> CTC --> B1 --> sink DAI0

# Include topology builder
include(`utils.m4')
include(`buffer.m4')
include(`pcm.m4')
include(`dai.m4')
include(`bytecontrol.m4')
include(`mixercontrol.m4')
include(`pipeline.m4')
include(`google_ctc_audio_processing.m4')

#
# Controls
#

# Host "Playback with codec adapter" PCM
# with 2 sink and 0 source periods
W_PCM_PLAYBACK(PCM_ID, CTC Processing Playback, 2, 0, SCHEDULE_CORE)

define(DEF_CTC_PRIV, concat(`ctc_priv_', PIPELINE_ID))
define(DEF_CTC_BYTES, concat(`ctc_bytes_', PIPELINE_ID))
include(`google_ctc_audio_processing_coef_default.m4')

# CTC Bytes control
C_CONTROLBYTES(DEF_CTC_BYTES, PIPELINE_ID,
	CONTROLBYTES_OPS(bytes,
		258 binds the mixer control to bytes get/put handlers,
		258, 258),
	CONTROLBYTES_EXTOPS(258 binds the mixer control to bytes get/put handlers,
		258, 258),
	, , ,
	CONTROLBYTES_MAX(void, 4244),
	,
	DEF_CTC_PRIV)

define(DEF_CTC_SWITCH, concat(`ctc_enable_', PIPELINE_ID))
define(`CONTROL_NAME', `DEF_CTC_SWITCH')
C_CONTROLMIXER(DEF_CTC_SWITCH, PIPELINE_ID,
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

W_GOOGLE_CTC_AUDIO_PROCESSING(0, PIPELINE_FORMAT, 2, 2, SCHEDULE_CORE,
    LIST(`      ', "DEF_CTC_BYTES"),
    LIST(`      ', "DEF_CTC_SWITCH"))

# Playback Buffers
W_BUFFER(0, COMP_BUFFER_SIZE(2,
    COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
    PLATFORM_HOST_MEM_CAP)
W_BUFFER(1, COMP_BUFFER_SIZE(DAI_PERIODS,
    COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
    PLATFORM_DAI_MEM_CAP)

#
# Pipeline Graph
#
#  host PCM_P --> B0 --> CTC -> B1 --> sink DAI0

P_GRAPH(pipe-ctc-playback, PIPELINE_ID,
	LIST(`		',
	`dapm(N_BUFFER(0), N_PCMP(PCM_ID))',
	`dapm(N_GOOGLE_CTC_AUDIO_PROCESSING(0), N_BUFFER(0))',
	`dapm(N_BUFFER(1), N_GOOGLE_CTC_AUDIO_PROCESSING(0))'))

#
# Pipeline Source and Sinks
#
indir(`define', concat(`PIPELINE_SOURCE_', PIPELINE_ID), N_BUFFER(1))
indir(`define', concat(`PIPELINE_PCM_', PIPELINE_ID), CTC Processing Playback PCM_ID)

#
# PCM Configuration
#

PCM_CAPABILITIES(CTC Processing Playback PCM_ID, CAPABILITY_FORMAT_NAME(PIPELINE_FORMAT), PCM_MIN_RATE, PCM_MAX_RATE, 2, PIPELINE_CHANNELS, 2, 16, 192, 16384, 65536, 65536)

undefine(`DEF_CTC_PRIV')
undefine(`DEF_CTC_BYTES')
undefine(`DEF_CTC_SWITCH')
