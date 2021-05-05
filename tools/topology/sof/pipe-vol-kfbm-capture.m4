# Passthrough Capture Pipeline with KPBM and PCM
#
# Pipeline Endpoints for connection are :-
#
#  host PCM_C <-+- KPBM 0 <-- B1 <-- Volume 0 <-- B0 <-- source DAI0
#               |
#  Others    <--+

# Include topology builder
include(`utils.m4')
include(`buffer.m4')
include(`pcm.m4')
include(`pga.m4')
include(`dai.m4')
include(`kpbm.m4')
include(`mixercontrol.m4')
include(`bytecontrol.m4')
include(`pipeline.m4')

dnl Soun Trigger Stream Name)
define(`N_STS', `Sound Trigger Capture '$1)

#
# Controls
#

# kpbm initial parameters, aligned with struct sof_kpb_config
CONTROLBYTES_PRIV(KPB_priv,
`       bytes "0x53,0x4f,0x46,0x00,0x00,0x00,0x00,0x00,'
`       0x18,0x00,0x00,0x00,0x00,0x10,0x00,0x03,'
`       0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'
`       0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'
`       0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'
`       0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'
`       0x80,0x3e,0x00,0x00,0x10,0x00,0x00,0x00"'
)

# KPB Bytes control with max value of 255
C_CONTROLBYTES(KPB, PIPELINE_ID,
	CONTROLBYTES_OPS(bytes, 258 binds the mixer control to bytes get/put handlers, 258, 258),
	CONTROLBYTES_EXTOPS(258 binds the mixer control to bytes get/put handlers, 258, 258),
	, , ,
	CONTROLBYTES_MAX(, 304),
	,
	KPB_priv)

# Volume Mixer control with max value of 80
C_CONTROLMIXER(KWD Capture Volume, PIPELINE_ID,
	CONTROLMIXER_OPS(volsw, 256 binds the mixer control to volume get/put handlers, 256, 256),
	CONTROLMIXER_MAX(, 80),
	false,
	CONTROLMIXER_TLV(TLV 80 steps from -50dB to +30dB for 1dB, vtlv_m50s1),
	Channel register and shift for Front Left/Right,
	VOLUME_CHANNEL_MAP)

#
# Volume configuration
#

W_VENDORTUPLES(capture_pga_tokens, sof_volume_tokens,
LIST(`		', `SOF_TKN_VOLUME_RAMP_STEP_TYPE	"0"'
     `		', `SOF_TKN_VOLUME_RAMP_STEP_MS		"250"'))

W_DATA(capture_pga_conf, capture_pga_tokens)

#
# Components and Buffers
#

# Host "Passthrough Capture" PCM
# with 0 sink and 2 source periods
W_PCM_CAPTURE(PCM_ID, Sound Trigger Capture, 0, 2, SCHEDULE_CORE)

# "Volume" has x source and 2 sink periods
W_PGA(0, PIPELINE_FORMAT, 2, DAI_PERIODS, capture_pga_conf, SCHEDULE_CORE,
	LIST(`		', "PIPELINE_ID KWD Capture Volume"))

# "KPBM" has 2 source and 2 sink periods
W_KPBM(0, PIPELINE_FORMAT, 2, 2, PIPELINE_ID, SCHEDULE_CORE,
	LIST(`             ', "KPB"))

# Capture Buffers
W_BUFFER(0, COMP_BUFFER_SIZE(DAI_PERIODS,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_DAI_MEM_CAP)
# Capture Buffers
W_BUFFER(1, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_HOST_MEM_CAP)
# Capture Buffers
W_BUFFER(2, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_HOST_MEM_CAP)

#
# Pipeline Graph
#
#  host PCM_C <-- B2 <--+- KPBM 0 <-- B1 <-- volume 0 <-- B0 <-- source DAI0
#                	|
#            Others  <--

P_GRAPH(pipe-kpbm-capture-PIPELINE_ID, PIPELINE_ID,
	LIST(`		',
	`dapm(N_PCMC(PCM_ID), N_BUFFER(2))',
	`dapm(N_BUFFER(2), N_KPBM(0, PIPELINE_ID))',
	`dapm(N_KPBM(0, PIPELINE_ID), N_BUFFER(1))',
	`dapm(N_BUFFER(1), N_PGA(0, PIPELINE_ID))',
	`dapm(N_PGA(0, PIPELINE_ID), N_BUFFER(0))'))

#
# Pipeline Source and Sinks
#
indir(`define', concat(`PIPELINE_SINK_', PIPELINE_ID), N_BUFFER(0))
indir(`define', concat(`PIPELINE_SOURCE_', PIPELINE_ID), N_KPBM(0, PIPELINE_ID))
indir(`define', concat(`PIPELINE_PCM_', PIPELINE_ID), N_PCMC(PCM_ID))

#
# PCM Configuration
#
dnl PCM_CAPTURE_LP_ADD(name, pipeline, capture)
PCM_CAPTURE_LP_ADD(DMIC_16k_PCM_NAME, PCM_ID, N_STS(PCM_ID))

PCM_CAPABILITIES(N_STS(PCM_ID), CAPABILITY_FORMAT_NAME(PIPELINE_FORMAT), 16000, 16000, 2, PIPELINE_CHANNELS, 2, 160, 192, 256000, 256000, 1280000)

