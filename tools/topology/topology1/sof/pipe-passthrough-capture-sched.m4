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
include(`pipeline.m4')

#
# Components and Buffers
#

# Host "Passthrough Capture" PCM
# with 0 sink and 2 source periods
W_PCM_CAPTURE(PCM_ID, Passthrough Capture, 0, DAI_PERIODS, SCHEDULE_CORE)

# Capture Buffers
W_BUFFER(0, COMP_BUFFER_SIZE(DAI_PERIODS,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_PASS_MEM_CAP, SCHEDULE_CORE)

# Virtual output widget
VIRTUAL_WIDGET(ECHO REF PIPELINE_ID, out_drv, PIPELINE_ID)

#
# Pipeline Graph
#
#  host PCM_C <-- B0 <-- sink DAI0

P_GRAPH(pipe-passthrough-capture-sched, PIPELINE_ID,
	LIST(`		',
	`dapm(N_PCMC(PCM_ID), N_BUFFER(0))',
	`dapm(N_BUFFER(0), ECHO REF PIPELINE_ID)'))
#
# Pipeline Source and Sinks
#
indir(`define', concat(`PIPELINE_SINK_', PIPELINE_ID), N_BUFFER(0))
indir(`define', concat(`PIPELINE_PCM_', PIPELINE_ID), Passthrough Capture PCM_ID)

W_PIPELINE(N_PCMC(PCM_ID), SCHEDULE_PERIOD, SCHEDULE_PRIORITY, SCHEDULE_CORE, SCHEDULE_TIME_DOMAIN, pipe_ll_schedule_plat)
#
# PCM Configuration
#

PCM_CAPABILITIES(Passthrough Capture PCM_ID, CAPABILITY_FORMAT_NAME(PIPELINE_FORMAT), 48000, 48000,  PIPELINE_CHANNELS, PIPELINE_CHANNELS, 2, 16, 192, 16384, 65536, 65536)
