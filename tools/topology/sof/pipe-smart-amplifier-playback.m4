# Smart amplifier playback Pipeline
#
#  Playback with smart_amp(Smart Amplifier), it will take the feedback(B2) from capture pipeline.
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
include(`smart_amp.m4')
include(`mixercontrol.m4')
include(`bytecontrol.m4')

ifdef(`SMART_TX_CHANNELS',`',`errprint(note: Need to define DAI TX channel number for sof-smart-amplifier
)')
ifdef(`SMART_FB_CHANNELS',`',`errprint(note: Need to define feedback channel number for sof-smart-amplifier
)')

#
# Controls
#

# initial config params for smart_amp, aligned with struct sof_smart_amp_config
CONTROLBYTES_PRIV(SMART_AMP_priv,
`       bytes "0x53,0x4f,0x46,0x00,0x00,0x00,0x00,0x00,'
`       0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x03,'
`       0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'
`       0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'
`       0x18,0x00,0x00,0x00,0x08,0x00,0x00,0x00,'
`		0x00,0x01,0xff,0xff,0xff,0xff,0xff,0xff,'
`		0xff,0xff,0x00,0x01,0xff,0xff,0xff,0xff"'
)

# Smart_amp Bytes control for config
C_CONTROLBYTES(Smart_amp Config, PIPELINE_ID,
        CONTROLBYTES_OPS(bytes, 258 binds the mixer control to bytes get/put handlers, 258, 258),
        CONTROLBYTES_EXTOPS(258 binds the mixer control to bytes get/put handlers, 258, 258),
        , , ,
        CONTROLBYTES_MAX(, 304),
        ,
        SMART_AMP_priv)

# Algorithm Model initial parameters
CONTROLBYTES_PRIV(MODEL_priv,
`       bytes "0x53,0x4f,0x46,0x00,0x01,0x00,0x00,0x00,'
`       0x00,0x00,0x00,0x00,0x00,0x10,0x00,0x03,'
`       0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'
`       0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00"'
)

# Detector Bytes control for Hotword Model blob
C_CONTROLBYTES(Smart_amp Model, PIPELINE_ID,
        CONTROLBYTES_OPS(bytes, 258 binds the mixer control to bytes get/put handlers, 258, 258),
        CONTROLBYTES_EXTOPS(258 binds the mixer control to bytes get/put handlers, 258, 258),
        , , ,
        CONTROLBYTES_MAX(, 300000),
        ,
        MODEL_priv)

#
# Components and Buffers
#

# Host "Low latency Playback" PCM
# with 2 sink and 0 source periods
W_PCM_PLAYBACK(PCM_ID, Smart Amplifier Playback, 2, 0)

# Mux 0 has 2 sink and source periods.
W_SMART_AMP(0, PIPELINE_FORMAT, 2, 2, LIST(`             ', "Smart_amp Config", "Smart_amp Model"))

# Low Latency Buffers
W_BUFFER(0, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_HOST_MEM_CAP)
W_BUFFER(1, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), SMART_TX_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_COMP_MEM_CAP)
W_BUFFER(2, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), SMART_FB_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_HOST_MEM_CAP)
#define REF buffer name for up layer connection
define(`N_SMART_REF_BUF',`BUF'PIPELINE_ID`.'2)

#
# Pipeline Graph
#
#  host PCM_P --B0--> smart_amp --B1--> sink DAI0
#			 ^
#			 |--B2--

P_GRAPH(pipe-smart-amp-playback-PIPELINE_ID, PIPELINE_ID,
	LIST(`		',
	`dapm(N_BUFFER(0), N_PCMP(PCM_ID))',
	`dapm(N_SMART_AMP(0), N_BUFFER(0))',
	`dapm(N_SMART_AMP(0), N_BUFFER(2))',
	`dapm(N_BUFFER(1), N_SMART_AMP(0))'))

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

