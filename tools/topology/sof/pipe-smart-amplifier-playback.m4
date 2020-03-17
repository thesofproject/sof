# Demux Pipeline
#
#  Playback with smart amplifier(dsm).
#
# Pipeline Endpoints for connection are :-
#
#	Playback smart_amp
#	B1 (DAI buffer)
#
#
#  host PCM_P -- B0 --> smart_amp -- B1--> sink DAI0
#			   ^
#			   |
#			   B2
#			   |

# Include topology builder
include(`utils.m4')
include(`buffer.m4')
include(`pcm.m4')
include(`pga.m4')
include(`dsm.m4')
include(`mixercontrol.m4')
include(`bytecontrol.m4')

#
# Components and Buffers
#

# Host "Low latency Playback" PCM
# with 2 sink and 0 source periods
W_PCM_PLAYBACK(PCM_ID, Smart Amplifier Playback, 2, 0)

# Mux 0 has 2 sink and source periods.
W_DSM(0, PIPELINE_FORMAT, 2, 2)

# Low Latency Buffers
W_BUFFER(0, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_HOST_MEM_CAP)
W_BUFFER(1, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_COMP_MEM_CAP)
W_BUFFER(2, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), 8, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_HOST_MEM_CAP)

#
# Pipeline Graph
#
#  host PCM_P --B0--> smart_amp --B1--> sink DAI0
#			 ^
#			 |--B2--

P_GRAPH(pipe-smart-amp-playback-PIPELINE_ID, PIPELINE_ID,
	LIST(`		',
	`dapm(N_BUFFER(0), N_PCMP(PCM_ID))',
	`dapm(N_DSM(0), N_BUFFER(0))',
	`dapm(N_DSM(0), N_BUFFER(2))',
	`dapm(N_BUFFER(1), N_DSM(0))'))

#
# Pipeline Source and Sinks
#
indir(`define', concat(`PIPELINE_SOURCE_', PIPELINE_ID), N_BUFFER(1))
indir(`define', concat(`PIPELINE_DEMUX_', PIPELINE_ID), N_MUXDEMUX(0))
indir(`define', concat(`PIPELINE_PCM_', PIPELINE_ID), Smart Amplifier Playback PCM_ID)

#
# PCM Configuration
#


# PCM capabilities supported by FW
PCM_CAPABILITIES(Smart Amplifier Playback PCM_ID, `S32_LE,S24_LE,S16_LE', 48000, 48000, 2, PIPELINE_CHANNELS, 2, 16, 192, 16384, 65536, 65536)

