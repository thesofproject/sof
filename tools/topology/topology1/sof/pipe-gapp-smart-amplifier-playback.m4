# Smart amplifier playback Pipeline
#
#  Playback with smart_amp(Smart Amplifier), it will take the feedback(B3) from capture pipeline.
#
# Pipeline Endpoints for connection are :-
#
#	Playback smart_amp
#	B1 (DAI buffer)
#
#
#  host PCM_P -- B0 --> GAPP -- B1 --> smart_amp -- B2--> sink DAI0
#			                  ^
#			                  |
#			                  B3
#			                  |

# Include topology builder
include(`utils.m4')
include(`buffer.m4')
include(`pcm.m4')
include(`pga.m4')
include(`smart_amp.m4')
include(`mixercontrol.m4')
include(`bytecontrol.m4')
include(`pipeline.m4')
include(`google_audio_post_processing.m4')

ifdef(`SMART_TX_CHANNELS',`',`fatal_error(note: Need to define DAI TX channel number for sof-smart-amplifier
)')
ifdef(`SMART_FB_CHANNELS',`',`fatal_error(note: Need to define feedback channel number for sof-smart-amplifier
)')

DECLARE_SOF_RT_UUID("smart_amp-test", smart_amp_comp_uuid, 0x167a961e, 0x8ae4,
		 0x11ea, 0x89, 0xf1, 0x00, 0x0c, 0x29, 0xce, 0x16, 0x35)
ifdef(`SMART_UUID',`', `define(`SMART_UUID', smart_amp_comp_uuid)');
#
# Controls
#

# initial config params for smart_amp, aligned with struct sof_smart_amp_config
ifelse(SMART_FB_CHANNELS, `8',
`define(`FB_CHMAP',`0xff,0xff,0x00,0x01,0xff,0xff,0xff,0xff')',
`define(`FB_CHMAP',`0x00,0x01,0x02,0x03,0xff,0xff,0xff,0xff')'
)

CONTROLBYTES_PRIV(SMART_AMP_priv,
`		bytes "0x53,0x4f,0x46,0x00,0x00,0x00,0x00,0x00,'
`		0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x03,'
`		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'
`		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'
`		0x18,0x00,0x00,0x00,DEC2HEX(SMART_FB_CHANNELS),0x00,0x00,0x00,'
`		0x00,0x01,0xff,0xff,0xff,0xff,0xff,0xff,'
`		FB_CHMAP"')

# Smart_amp Bytes control for config
C_CONTROLBYTES(Smart_amp Config, PIPELINE_ID,
        CONTROLBYTES_OPS(bytes, 258 binds the mixer control to bytes get/put handlers, 258, 258),
        CONTROLBYTES_EXTOPS(258 binds the mixer control to bytes get/put handlers, 258, 258),
        , , ,
        CONTROLBYTES_MAX(, 304),
        ,
        SMART_AMP_priv)

# Algorithm Model initial parameters, just empty one at this moment.
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


ifelse(SOF_ABI_VERSION_3_17_OR_GRT, `1',
C_CONTROLBYTES_VOLATILE_READONLY(Smart_amp Model_Get_params, PIPELINE_ID,
        CONTROLBYTES_OPS(bytes, 258 binds the mixer control to bytes get handlers, 258, 258),
        CONTROLBYTES_EXTOPS_READONLY(260 binds the mixer control to bytes get handlers, 260),
        , , ,
        CONTROLBYTES_MAX(, 300000),
        ,
        MODEL_priv))

# GAPP Parameters
CONTROLBYTES_PRIV(GAPP_priv,
`       bytes "0x53,0x4f,0x46,0x00,0x00,0x00,0x00,0x00,'
`       0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x03,'
`       0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'
`       0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'
`       0x01,0x00,0x00,0x00,0x01,0x00,0x00,0x00"'
)

C_CONTROLBYTES(GAPP, PIPELINE_ID,
     CONTROLBYTES_OPS(bytes, 258 binds the control to bytes get/put handlers, 258, 258),
     CONTROLBYTES_EXTOPS(258 binds the control to bytes get/put handlers, 258, 258),
     , , ,
     CONTROLBYTES_MAX(, 140),
     ,
     GAPP_priv)

C_CONTROLBYTES(GAPP Model, PIPELINE_ID,
     CONTROLBYTES_OPS(bytes, 258 binds the control to bytes get/put handlers, 258, 258),
     CONTROLBYTES_EXTOPS(258 binds the control to bytes get/put handlers, 258, 258),
     , , ,
     CONTROLBYTES_MAX(, 300000),
     ,
     MODEL_priv)

CONTROLBYTES_PRIV(DATA_priv,
`       bytes "0x53,0x4f,0x46,0x00,0x02,0x00,0x00,0x00,'
`       0x00,0x00,0x00,0x00,0x00,0x10,0x00,0x03,'
`       0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'
`       0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00"'
)
ifelse(SOF_ABI_VERSION_3_17_OR_GRT, `1',
C_CONTROLBYTES_VOLATILE_READONLY(GAPP Model_Get_params, PIPELINE_ID,
        CONTROLBYTES_OPS(bytes, 258 binds the mixer control to bytes get handlers, 258, 258),
        CONTROLBYTES_EXTOPS_READONLY(260 binds the mixer control to bytes get handlers, 260),
        , , ,
        CONTROLBYTES_MAX(, 300000),
        ,
        DATA_priv))

C_CONTROLMIXER(GAPP Playback Volume, PIPELINE_ID,
        CONTROLMIXER_OPS(volsw, 256 binds the mixer control to volume get/put handlers, 256, 256),
	CONTROLMIXER_MAX(, 32),
	false,
	CONTROLMIXER_TLV(TLV 32 steps from -64dB to -0dB for 2dB, vtlv_m64s2),
	Channel register and shift for Front Left/Right,
	VOLUME_CHANNEL_MAP)

#
# Components and Buffers
#

# Host "Low latency Playback" PCM
# with 2 sink and 0 source periods
W_PCM_PLAYBACK(PCM_ID, Smart Amplifier Playback, 2, 0, SCHEDULE_CORE)

# "GAPP" has 2 sink periods and 2 source periods
W_GAPP(0, PIPELINE_FORMAT, 2, 2, SCHEDULE_CORE,
     LIST(`           ', "PIPELINE_ID GAPP Playback Volume"),
     LIST(`           ', "GAPP", "GAPP Model",
     ifelse(SOF_ABI_VERSION_3_17_OR_GRT, `1', "GAPP Model_Get_params")))

# Mux 0 has 2 sink and source periods.
W_SMART_AMP(0, SMART_UUID, PIPELINE_FORMAT, 2, 2, SCHEDULE_CORE,
	    LIST(`             ', "Smart_amp Config", "Smart_amp Model",
	    ifelse(SOF_ABI_VERSION_3_17_OR_GRT, `1', "Smart_amp Model_Get_params")))

# Low Latency Buffers
W_BUFFER(0, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_HOST_MEM_CAP)
W_BUFFER(1, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_COMP_MEM_CAP)
W_BUFFER(2, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), SMART_TX_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_HOST_MEM_CAP)
W_BUFFER(3, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), SMART_FB_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_HOST_MEM_CAP)
#define REF buffer name for up layer connection
define(`N_SMART_REF_BUF',`BUF'PIPELINE_ID`.'3)

#
# Pipeline Graph
#
#  host PCM_P --B0--> GAPP --B1--> smart_amp --B2--> sink DAI0
#	  		              ^
#			              |--B3--

P_GRAPH(pipe-smart-amp-playback-PIPELINE_ID, PIPELINE_ID,
	LIST(`		',
	`dapm(N_BUFFER(0), N_PCMP(PCM_ID))',
	`dapm(N_GAPP(0), N_BUFFER(0))',
	`dapm(N_BUFFER(1), N_GAPP(0))',
	`dapm(N_SMART_AMP(0), N_BUFFER(1))',
	`dapm(N_SMART_AMP(0), N_BUFFER(3))',
	`dapm(N_BUFFER(2), N_SMART_AMP(0))'))

#
# Pipeline Source and Sinks
#
indir(`define', concat(`PIPELINE_SOURCE_', PIPELINE_ID), N_BUFFER(2))
indir(`define', concat(`PIPELINE_DEMUX_', PIPELINE_ID), N_MUXDEMUX(0))
indir(`define', concat(`PIPELINE_PCM_', PIPELINE_ID), Smart Amplifier Playback PCM_ID)

#
# PCM Configuration
#

# PCM capabilities supported by FW
PCM_CAPABILITIES(Smart Amplifier Playback PCM_ID,
	CAPABILITY_FORMAT_NAME(PIPELINE_FORMAT),
	48000, 48000, dnl rate_min, rate_max
	SMART_PB_CH_NUM, SMART_PB_CH_NUM, dnl channels_min, channels_max
	2, 16, dnl periods_min, periods_max
	192, 16384, dnl period_size_min, period_size_max
	65536, 65536)

