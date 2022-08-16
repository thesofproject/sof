# Mixer volume Playback connector
#
#  pipeline playback starting with a LL mixer
#
# Pipeline Endpoints for connection are :-
#
#	LL Playback Mixer (Mixer)
#	LL Playback Volume B1
#
# ll mixer(M) --> B0 --> volume(LL) --> B1

# Include topology builder
include(`utils.m4')
include(`mixer.m4')
include(`mixercontrol.m4')
include(`pga.m4')
include(`buffer.m4')
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

# Mixer 0 has 2 sink and source periods.
W_MIXER(0, PIPELINE_FORMAT, 2, 2, SCHEDULE_CORE)

# "Playback Volume" has 2 source and x sink periods for DAI ping-pong
W_PGA(0, PIPELINE_FORMAT, SCHEDULE_PERIODS, 2, DEF_PGA_CONF, SCHEDULE_CORE,
	LIST(`		', "PIPELINE_ID Playback Volume"))

#
# Pipeline Configuration.
#

W_PIPELINE(SCHED_COMP, SCHEDULE_PERIOD, SCHEDULE_PRIORITY, SCHEDULE_CORE, SCHEDULE_TIME_DOMAIN, pipe_media_schedule_plat)

# Low Latency Buffers - both buffers use the pipeline format
W_BUFFER(0, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PIPELINE_RATE, SCHEDULE_PERIOD)),
	PLATFORM_COMP_MEM_CAP)
W_BUFFER(1, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PIPELINE_RATE, SCHEDULE_PERIOD)),
	PLATFORM_COMP_MEM_CAP)

#
# Graph connections to pipelines
#
P_GRAPH(pipe-mixer-volume-playback, PIPELINE_ID,
	LIST(`		',
	`dapm(N_BUFFER(0), N_MIXER(0))',
	`dapm(N_PGA(0), N_BUFFER(0))',
	`dapm(N_BUFFER(1), N_PGA(0))'))

indir(`define', concat(`PIPELINE_PLAYBACK_SCHED_COMP_', PIPELINE_ID), SCHED_COMP)
indir(`define', concat(`PIPELINE_MIXER_SINK_', PIPELINE_ID), N_MIXER(0))
indir(`define', concat(`PIPELINE_MIXER_SOURCE_', PIPELINE_ID), N_BUFFER(1))

undefine(`DEF_PGA_TOKENS')
undefine(`DEF_PGA_CONF')
