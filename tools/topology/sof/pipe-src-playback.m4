# Low Latency Passthrough with volume Pipeline and PCM
#
# Pipeline Endpoints for connection are :-
#
#  host PCM_P --> B0 --> SRC --> B1 --> sink DAI0

# Include topology builder
include(`utils.m4')
include(`buffer.m4')
include(`pcm.m4')
include(`src.m4')
include(`dai.m4')
include(`pipeline.m4')

#
# Controls
#

#
# Components and Buffers
#

# Host "SRC Playback" PCM
# with 3 sink and 0 source periods
W_PCM_PLAYBACK(PCM_ID, SRC Playback, 3, 0)

#
# SRC Configuration
#

W_VENDORTUPLES(media_src_tokens, sof_src_tokens, LIST(`		', `SOF_TKN_SRC_RATE_OUT	"PIPELINE_RATE"'))

W_DATA(media_src_conf, media_src_tokens)

# "SRC" has 3 source and 3 sink periods
W_SRC(0, PIPELINE_FORMAT, 3, 3, media_src_conf)

# Playback Buffers
W_BUFFER(0, COMP_BUFFER_SIZE(3,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_HOST_MEM_CAP)
W_BUFFER(1, COMP_BUFFER_SIZE(3,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_HOST_MEM_CAP)
W_BUFFER(2, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_DAI_MEM_CAP)

#
# Pipeline Graph
#
#  host PCM_P --> B0 --> SRC 0 --> B1 --> sink DAI0

P_GRAPH(pipe-pass-src-playback-PIPELINE_ID, PIPELINE_ID,
	LIST(`		',
	`dapm(N_BUFFER(0), N_PCMP(PCM_ID))',
	`dapm(N_SRC(0), N_BUFFER(0))',
	`dapm(N_BUFFER(1), N_SRC(0))'))

#
# Pipeline Source and Sinks
#
indir(`define', concat(`PIPELINE_SOURCE_', PIPELINE_ID), N_BUFFER(1))
indir(`define', concat(`PIPELINE_PCM_', PIPELINE_ID), SRC Playback PCM_ID)

#
# PCM Configuration
#

PCM_CAPABILITIES(SRC Playback PCM_ID, `S32_LE,S24_LE,S16_LE', PCM_MIN_RATE, PCM_MAX_RATE, 2, PIPELINE_CHANNELS, 2, 16, 192, 16384, 65536, 65536)


