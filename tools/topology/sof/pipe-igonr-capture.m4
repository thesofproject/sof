# Capture Passthrough Pipeline and PCM
#
# Pipeline Endpoints for connection are :-
#
#  host PCM_C <-- B0 <-- sink DAI0

# Include topology builder
include(`utils.m4')
include(`buffer.m4')
include(`pcm.m4')
include(`dai.m4')
include(`bytecontrol.m4')
include(`pipeline.m4')
include(`igo_nr.m4')

CONTROLBYTES_PRIV(IGO_NR_priv,
`       bytes "0x53,0x4f,0x46,0x00,'
`       0x00,0x00,0x00,0x00,'
`       0x40,0x00,0x00,0x00,'
`       0x00,0x00,0x00,0x03,'
`       0x00,0x00,0x00,0x00,'
`       0x00,0x00,0x00,0x00,'
`       0x00,0x00,0x00,0x00,'
`       0x00,0x00,0x00,0x00,'
`       0x00,0x20,0x00,0x00,'
`       0x00,0x00,0x00,0x00,'
`       0x00,0x00,0x00,0x00,'
`       0x01,0x00,0x00,0x00,'
`       0x01,0x00,0x00,0x00,'
`       0x01,0x00,0x00,0x00,'
`       0x01,0x00,0x00,0x00,'
`       0x02,0x00,0x00,0x00,'
`       0x02,0x00,0x00,0x00,'
`       0x02,0x00,0x00,0x00,'
`       0x01,0x00,0x00,0x00,'
`       0x3d,0x00,0x00,0x00,'
`       0x09,0x00,0x00,0x00,'
`       0x00,0x00,0x00,0x00,'
`       0x00,0x20,0x00,0x00,'
`       0x34,0x03,0x00,0x00"'
)

C_CONTROLBYTES(IGO_NR, PIPELINE_ID,
     CONTROLBYTES_OPS(bytes, 258 binds the control to bytes get/put handlers, 258, 258),
     CONTROLBYTES_EXTOPS(258 binds the control to bytes get/put handlers, 258, 258),
     , , ,
     CONTROLBYTES_MAX(, 196),
     ,
     IGO_NR_priv)

#
# Components and Buffers
#

# Host "Passthrough Capture" PCM
# with 0 sink and 2 source periods
W_PCM_CAPTURE(PCM_ID, Passthrough Capture, 0, DAI_PERIODS, SCHEDULE_CORE)

W_IGO_NR(0, PIPELINE_FORMAT, 2, DAI_PERIODS, SCHEDULE_CORE, LIST(`         ', "IGO_NR"))

# Capture Buffers
W_BUFFER(0, COMP_BUFFER_SIZE(DAI_PERIODS,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_PASS_MEM_CAP)

W_BUFFER(1, COMP_BUFFER_SIZE(DAI_PERIODS,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_PASS_MEM_CAP)
#
# Pipeline Graph
#
#  host PCM_C <-- B0 <-- NR0 <-- B1<-- sink DAI0

P_GRAPH(pipe-nr-capture-PIPELINE_ID, PIPELINE_ID,
	LIST(`		',
	`dapm(N_PCMC(PCM_ID), N_BUFFER(0))',
	`dapm(N_BUFFER(0), N_IGO_NR(0))',
	`dapm(N_IGO_NR(0), N_BUFFER(1))'))

#
# Pipeline Source and Sinks
#
indir(`define', concat(`PIPELINE_SINK_', PIPELINE_ID), N_BUFFER(1))
indir(`define', concat(`PIPELINE_PCM_', PIPELINE_ID), Passthrough Capture PCM_ID)

#
# PCM Configuration
#

PCM_CAPABILITIES(Passthrough Capture PCM_ID, COMP_FORMAT_NAME(PIPELINE_FORMAT), PCM_MIN_RATE, PCM_MAX_RATE,  PIPELINE_CHANNELS, PIPELINE_CHANNELS, 2, 16, 192, 16384, 65536, 65536)
