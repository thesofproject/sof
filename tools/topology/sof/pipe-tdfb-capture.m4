# Capture TDFB Pipeline and PCM, 48 kHz
#
# Pipeline Endpoints for connection are :-
#
#  host PCM_C <-- B0 <-- TDFB 0 <-- B1 <-- sink DAI0

# Include topology builder
include(`utils.m4')
include(`buffer.m4')
include(`pcm.m4')
include(`dai.m4')
include(`pipeline.m4')
include(`bytecontrol.m4')
include(`enumcontrol.m4')
include(`tdfb.m4')

#
# Controls
#

# defines with pipeline ID appended for unique names
include(`tdfb_defines.m4')

# Define filter. A passthrough configuration is set by default.
ifdef(`PIPELINE_FILTER1', , `define(PIPELINE_FILTER1, `tdfb/coef_line2_pass.m4')')
include(PIPELINE_FILTER1)

# Include controls: binary, enums
# byte control in DEF_TDFB_BYTES
# enum controls in DEF_TDFB_ONOFF_ENUM, DEF_TDFB_DIRECTION_ENUM
include(`tdfb_controls.m4')

#
# Components and Buffers
#

# Host "TDFB Capture" PCM
# with 0 sink and 2 source periods
W_PCM_CAPTURE(PCM_ID, TDFB Capture, 0, 2)

# "TDFB 0" has 2 sink period and x source periods
W_TDFB(0, PIPELINE_FORMAT, 2, DAI_PERIODS, SCHEDULE_CORE,
	LIST(`		', "DEF_TDFB_BYTES"),
	LIST(`		', "DEF_TDFB_ONOFF_ENUM"),
	LIST(`		', "DEF_TDFB_DIRECTION_ENUM"))

# Capture Buffers
W_BUFFER(0, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS,
	COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_PASS_MEM_CAP)

W_BUFFER(1, COMP_BUFFER_SIZE(DAI_PERIODS,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS,
	COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_PASS_MEM_CAP)

#
# Pipeline Graph
#
#  host PCM_C <-- B0 <-- TDFB 0 <-- B1 <-- sink DAI0

P_GRAPH(pipe-pass-capture-PIPELINE_ID, PIPELINE_ID,
	LIST(`		',
	`dapm(N_PCMC(PCM_ID), N_BUFFER(0))',
	`dapm(N_BUFFER(0), N_TDFB(0))',
	`dapm(N_TDFB(0), N_BUFFER(1))'))

#
# Pipeline Source and Sinks
#
indir(`define', concat(`PIPELINE_SINK_', PIPELINE_ID), N_BUFFER(1))
indir(`define', concat(`PIPELINE_PCM_', PIPELINE_ID), TDFB Capture PCM_ID)

#
# PCM Configuration
#

PCM_CAPABILITIES(TDFB Capture PCM_ID, `S32_LE,S24_LE,S16_LE', PCM_MIN_RATE,
	PCM_MAX_RATE, PIPELINE_CHANNELS, PIPELINE_CHANNELS,
	2, 16, 192, 16384, 65536, 65536)

include(`tdfb_undefines.m4')
