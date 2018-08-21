# Low Latency Passthrough with volume Pipeline and PCM
#
# Pipeline Endpoints for connection are :-
#
#  host PCM_P --> SRC --> sink DAI0

# Include topology builder
include(`utils.m4')
include(`src.m4')
include(`buffer.m4')
include(`pcm.m4')
include(`dai.m4')
include(`pipeline.m4')

#
# Components and Buffers
#

# Host "Passthrough Playback" PCM
# with 3 sink and 0 source periods
W_PCM_PLAYBACK(PCM_ID, Passthrough Playback, 3, 0, 2)

#
# SRC Configuration
#

W_VENDORTUPLES(media_src_tokens, sof_src_tokens, LIST(`		', `SOF_TKN_SRC_RATE_OUT	"48000"'))

W_DATA(media_src_conf, media_src_tokens)

# "SRC" has 3 source and 3 sink periods
W_SRC(0, PIPELINE_FORMAT, 3, 3, media_src_conf, 2)

# Playback Buffers
W_BUFFER(0, COMP_BUFFER_SIZE(3,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, SCHEDULE_FRAMES),
	PLATFORM_HOST_MEM_CAP)
W_BUFFER(1, COMP_BUFFER_SIZE(3,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, SCHEDULE_FRAMES),
	PLATFORM_DAI_MEM_CAP)

#
# Pipeline Graph
#
#  host PCM_P --> B0 --> SRC 0 --> B1 --> sink DAI0

P_GRAPH(pipe-pass-src-playback-PIPELINE_ID, PIPELINE_ID,
	LIST(`		',
	`dapm(N_PCMP(PCM_ID), Passthrough Playback PCM_ID)',
	`dapm(N_BUFFER(0), N_PCMP(PCM_ID))',
	`dapm(N_SRC(0), N_BUFFER(0))',
	`dapm(N_BUFFER(1), N_SRC(0))'))

#
# Pipeline Source and Sinks
#
indir(`define', concat(`PIPELINE_SOURCE_', PIPELINE_ID), N_BUFFER(1))
indir(`define', concat(`PIPELINE_PCM_', PIPELINE_ID), Passthrough Playback PCM_ID)

#
# PCM Configuration
#

PCM_CAPABILITIES(Passthrough Playback PCM_ID, `S32_LE,S24_LE,S16_LE', 8000, 192000, 2, 8, 2, 16, 192, 16384, 65536, 65536)

