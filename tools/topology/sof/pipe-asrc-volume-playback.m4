# Low Latency Passthrough with volume Pipeline and PCM
#
# Pipeline Endpoints for connection are :-
#
#  host PCM_P --> ASRC --> Volume --> sink DAI0

# Include topology builder
include(`utils.m4')
include(`asrc.m4')
include(`buffer.m4')
include(`pcm.m4')
include(`pga.m4')
include(`dai.m4')
include(`mixercontrol.m4')
include(`pipeline.m4')

#
# Controls
#
# Volume Mixer control with max value of 32
C_CONTROLMIXER(Master Playback Volume, PIPELINE_ID,
	CONTROLMIXER_OPS(volsw,
		256 binds the mixer control to volume get/put handlers,
		256, 256),
	CONTROLMIXER_MAX(, 32),
	false,
	CONTROLMIXER_TLV(TLV 32 steps from -64dB to 0dB for 2dB, vtlv_m64s2),
	Channel register and shift for Front Left/Right,
	VOLUME_CHANNEL_MAP)

#
# Components and Buffers
#

# Host "ASRC Playback" PCM
# with 2 sink and 0 source periods
W_PCM_PLAYBACK(PCM_ID, ASRC Playback, 2, 0, SCHEDULE_CORE)

#
# ASRC Configuration
#

define(MY_ASRC_TOKENS, concat(`asrc_tokens_', PIPELINE_ID))
define(MY_ASRC_CONF, concat(`asrc_conf_', PIPELINE_ID))
W_VENDORTUPLES(MY_ASRC_TOKENS, sof_asrc_tokens,
LIST(`		', `SOF_TKN_ASRC_RATE_OUT "PIPELINE_RATE"'
     `		', `SOF_TKN_ASRC_ASYNCHRONOUS_MODE "1"'
     `		', `SOF_TKN_ASRC_OPERATION_MODE "0"'))
W_DATA(MY_ASRC_CONF, MY_ASRC_TOKENS)

# "ASRC" has 2 sink and 2 source periods
W_ASRC(0, PIPELINE_FORMAT, 2, 2, MY_ASRC_CONF)

#
# Volume Configuration
#
define(MY_PGA_TOKENS, concat(`pga_tokens_', PIPELINE_ID))
define(MY_PGA_CONF, concat(`pga_conf_', PIPELINE_ID))
W_VENDORTUPLES(MY_PGA_TOKENS, sof_volume_tokens,
LIST(`		', `SOF_TKN_VOLUME_RAMP_STEP_TYPE	"2"'
     `		', `SOF_TKN_VOLUME_RAMP_STEP_MS		"20"'))
W_DATA(MY_PGA_CONF, MY_PGA_TOKENS)

# "Volume" has x sink and 2 source periods
W_PGA(0, PIPELINE_FORMAT, DAI_PERIODS, 2, MY_PGA_CONF, SCHEDULE_CORE,
	LIST(`		', "PIPELINE_ID Master Playback Volume"))

# Playback Buffers
W_BUFFER(0, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS,
	COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_HOST_MEM_CAP)
W_BUFFER(1, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS,
	COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_HOST_MEM_CAP)
W_BUFFER(2, COMP_BUFFER_SIZE(DAI_PERIODS,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS,
	COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_DAI_MEM_CAP)

#
# Pipeline Graph
#
#  host PCM_P --> B0 --> ASRC --> B1 --> PGA --> B2 --> sink DAI0

P_GRAPH(pipe-asrc-volume-playback-PIPELINE_ID, PIPELINE_ID,
	LIST(`		',
	`dapm(N_BUFFER(0), N_PCMP(PCM_ID))',
	`dapm(N_ASRC(0), N_BUFFER(0))',
	`dapm(N_BUFFER(1), N_ASRC(0))',
	`dapm(N_PGA(0), N_BUFFER(1))',
	`dapm(N_BUFFER(2), N_PGA(0))'))

#
# Pipeline Source and Sinks
#
indir(`define', concat(`PIPELINE_SOURCE_', PIPELINE_ID), N_BUFFER(2))
indir(`define', concat(`PIPELINE_PCM_', PIPELINE_ID), ASRC Playback PCM_ID)

#
# PCM Configuration
#

PCM_CAPABILITIES(ASRC Playback PCM_ID, CAPABILITY_FORMAT_NAME(PIPELINE_FORMAT), PCM_MIN_RATE,
	PCM_MAX_RATE, 2, PIPELINE_CHANNELS, 2, 16, 192, 16384, 65536, 65536)

undefine(`MY_ASRC_TOKENS')
undefine(`MY_ASRC_CONF')
undefine(`MY_PGA_TOKENS')
undefine(`MY_PGA_CONF')
