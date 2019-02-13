# Sound Detector
#
#  Generic sound detector.
#
# Pipeline Endpoints for connection are :-
#
#  (Sound Detector <-- Channel Selector) <-- Key Phrase Buffer Manager <--- Source Pipeline
#

# Include topology builder
include(`utils.m4')
include(`buffer.m4')
include(`pga.m4')
include(`ch_sel.m4')
include(`detect.m4')
include(`pipeline.m4')

#
# Controls
#


#
# Components and Buffers
#

# "Detect 0" has 2 sink period and 0 source periods
W_DETECT(0, PIPELINE_FORMAT, 0, 2, KEYWORD, N_STS(PCM_ID))

W_SELECTOR(0, PIPELINE_FORMAT, 2, 2, 2, 1, 0)

# Capture Buffers
W_BUFFER(1, COMP_BUFFER_SIZE(2,
	 COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, SCHEDULE_FRAMES),
	 PLATFORM_HOST_MEM_CAP)
# Capture Buffers
W_BUFFER(2, COMP_BUFFER_SIZE(2,
	 COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, SCHEDULE_FRAMES),
	 PLATFORM_HOST_MEM_CAP)
# Virtual output widget
VIRTUAL_WIDGET(DETECT SINK PIPELINE_ID, out_drv, PIPELINE_ID)

# Pipeline
dnl W_PIPELINE(stream, deadline, priority, frames, core, timer, platform)
W_PIPELINE(SCHED_COMP, SCHEDULE_DEADLINE, SCHEDULE_PRIORITY, SCHEDULE_FRAMES, SCHEDULE_CORE, 0, pipe_media_schedule_plat)

#
# Pipeline Graph
#
# Detect 0 <-- B2 <-- Channel Selector 0 <-- B1

P_GRAPH(pipe-detect-PIPELINE_ID, PIPELINE_ID,
	LIST(`		',
	`dapm(DETECT SINK PIPELINE_ID, N_DETECT(0))',
	`dapm(N_DETECT(0), N_BUFFER(2))',
	`dapm(N_BUFFER(2), N_SELECTOR(0))',
	`dapm(N_SELECTOR(0), N_BUFFER(1))'))

#
# Pipeline Source and Sinks
#
indir(`define', concat(`PIPELINE_SINK_', PIPELINE_ID), N_BUFFER(1))
indir(`define', concat(`PIPELINE_DETECT_', PIPELINE_ID), DETECT SINK PIPELINE_ID)
