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
include(`pga.m4')
include(`dai.m4')
include(`mixercontrol.m4')
include(`pipeline.m4')

#
# Controls
#
# Volume Mixer control with max value of 32
C_CONTROLMIXER(Master Playback Volume, PIPELINE_ID,
	CONTROLMIXER_OPS(volsw, 256 binds the mixer control to volume get/put handlers, 256, 256),
	CONTROLMIXER_MAX(, 32),
	false,
	CONTROLMIXER_TLV(TLV 32 steps from -90dB to +6dB for 3dB, vtlv_m90s3),
	Channel register and shift for Front Left/Right,
	LIST(`	', KCONTROL_CHANNEL(FL, 1, 0), KCONTROL_CHANNEL(FR, 1, 1)))

#
# Components and Buffers
#

# Host "SRC Playback" PCM
# with 3 sink and 0 source periods
W_PCM_PLAYBACK(PCM_ID, SRC Playback, 3, 0, 2)

#
# SRC Configuration
#

W_VENDORTUPLES(media_src_tokens, sof_src_tokens, LIST(`		', `SOF_TKN_SRC_RATE_OUT	"48000"'))

W_DATA(media_src_conf, media_src_tokens)

# "SRC" has 3 source and 3 sink periods
W_SRC(0, PIPELINE_FORMAT, 3, 3, media_src_conf, 2)

# "Volume" has 2 source and 2 sink periods
W_PGA(0, PIPELINE_FORMAT, 2, 2, 2, LIST(`		', "PIPELINE_ID Master Playback Volume"))

# Playback Buffers
W_BUFFER(0, COMP_BUFFER_SIZE(3,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, SCHEDULE_FRAMES),
	PLATFORM_HOST_MEM_CAP)
W_BUFFER(1, COMP_BUFFER_SIZE(3,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, SCHEDULE_FRAMES),
	PLATFORM_HOST_MEM_CAP)
W_BUFFER(2, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, SCHEDULE_FRAMES),
	PLATFORM_DAI_MEM_CAP)

#
# Pipeline Graph
#
#  host PCM_P --> B0 --> SRC 0 --> B1 Volume 0 --> B2 --> sink DAI0

P_GRAPH(pipe-pass-src-playback-PIPELINE_ID, PIPELINE_ID,
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
indir(`define', concat(`PIPELINE_PCM_', PIPELINE_ID), SRC Playback PCM_ID)

#
# PCM Configuration
#

PCM_CAPABILITIES(SRC Playback PCM_ID, `S32_LE,S24_LE,S16_LE', 8000, 192000, 2, PIPELINE_CHANNELS, 2, 16, 192, 16384, 65536, 65536)

