# Host PCM Volume playback pipeline
#
# Pipeline Endpoints for connection are :-
#
#  host PCM_P --> B0 --> SRC 0 --> B1 --> Volume 0 --> B2 --> [other pipeline]

# Include topology builder
include(`utils.m4')
include(`buffer.m4')
include(`pcm.m4')
include(`src.m4')
include(`pga.m4')
include(`dai.m4')
include(`mixercontrol.m4')
include(`pipeline.m4')

#
# Controls
#
# Volume Mixer control with max value of 32
C_CONTROLMIXER(Playback Volume, PIPELINE_ID,
	CONTROLMIXER_OPS(volsw, 256 binds the mixer control to volume get/put handlers, 256, 256),
	CONTROLMIXER_MAX(, 32),
	false,
	CONTROLMIXER_TLV(TLV 32 steps from -64dB to 0dB for 2dB, vtlv_m64s2),
	Channel register and shift for Front Left/Right,
	VOLUME_CHANNEL_MAP)

#
# Volume configuration
#

define(DEF_PGA_TOKENS, concat(`pga_tokens_', PIPELINE_ID))
define(DEF_PGA_CONF, concat(`pga_conf_', PIPELINE_ID))

W_VENDORTUPLES(DEF_PGA_TOKENS, sof_volume_tokens,
LIST(`		', `SOF_TKN_VOLUME_RAMP_STEP_TYPE	"0"'
     `		', `SOF_TKN_VOLUME_RAMP_STEP_MS		"20"'))

W_DATA(DEF_PGA_CONF, DEF_PGA_TOKENS)

define(DEF_SRC_TOKENS, concat(`src_tokens_', PIPELINE_ID))
define(DEF_SRC_CONF, concat(`src_conf_', PIPELINE_ID))

W_VENDORTUPLES(DEF_SRC_TOKENS, sof_src_tokens,
LIST(`		', `SOF_TKN_SRC_RATE_OUT	"PIPELINE_RATE"'))

W_DATA(DEF_SRC_CONF, DEF_SRC_TOKENS)

#
# Components and Buffers
#

# Host "Playback" PCM
# with 2 sink and 0 source periods
W_PCM_PLAYBACK(PCM_ID, Playback, 2, 0, SCHEDULE_CORE)

# "SRC" has 2 source and 2 sink periods
W_SRC(0, PIPELINE_FORMAT, 2, 2, DEF_SRC_CONF)

# "Volume" has 2 source and x sink periods
W_PGA(0, PIPELINE_FORMAT, DAI_PERIODS, 2, DEF_PGA_CONF, SCHEDULE_CORE,
	LIST(`		', "PIPELINE_ID Playback Volume"))

# Playback Buffers
W_BUFFER(0, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_HOST_MEM_CAP, SCHEDULE_CORE)
W_BUFFER(1, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_HOST_MEM_CAP, SCHEDULE_CORE)
W_BUFFER(2, COMP_BUFFER_SIZE(DAI_PERIODS,
	COMP_SAMPLE_SIZE(DAI_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_DAI_MEM_CAP, SCHEDULE_CORE)

#
# Pipeline Graph
#
#  host PCM_P --> B0 -- SRC 0 --> B1 --> Volume 0 --> B2 --> sink DAI0

P_GRAPH(pipe-host-volume-playback, PIPELINE_ID,
	LIST(`		',
	`dapm(N_BUFFER(0), N_PCMP(PCM_ID))',
	`dapm(N_SRC(0), N_BUFFER(0))',
	`dapm(N_BUFFER(1), N_SRC(0))',
	`dapm(N_PGA(0), N_BUFFER(1))',
	`dapm(N_BUFFER(2), N_PGA(0))'))

#
# Pipeline Source and Sinks
#
indir(`define', concat(`PIPELINE_SOURCE_', PIPELINE_ID), N_BUFFER(2))
indir(`define', concat(`PIPELINE_PCM_', PIPELINE_ID), Playback PCM_ID)

#
# Pipeline Configuration.
#

W_PIPELINE(SCHED_COMP, SCHEDULE_PERIOD, SCHEDULE_PRIORITY, SCHEDULE_CORE, SCHEDULE_TIME_DOMAIN, pipe_media_schedule_plat)

#
# PCM Configuration

#
PCM_CAPABILITIES(Playback PCM_ID, CAPABILITY_FORMAT_NAME(PIPELINE_FORMAT), 8000, 192000, 2, PIPELINE_CHANNELS, 2, 16, 192, 16384, 65536, 65536)

undefine(`DEF_PGA_TOKENS')
undefine(`DEF_PGA_CONF')
undefine(`DEF_SRC_TOKENS')
undefine(`DEF_SRC_CONF')
