#
# Topology for Tigerlake with rt711 + rt1308 (x2).
#

# if XPROC is not defined, define with default pipe
ifdef(`DMICPROC', , `define(DMICPROC, eq-iir-volume)')
ifdef(`DMIC16KPROC', , `define(DMIC16KPROC, eq-iir-volume)')

# Include topology builder
include(`utils.m4')
include(`dai.m4')
include(`pipeline.m4')
include(`alh.m4')
include(`hda.m4')
include(`platform/intel/dmic.m4')

# Include TLV library
include(`common/tlv.m4')

# Include Token library
include(`sof/tokens.m4')

include(`platform/intel/'PLATFORM`.m4')

ifdef(`NO_JACK',
`
define(JACK_OFFSET, `0')
',
`
define(JACK_OFFSET, `2')
')

ifdef(`AMP_1_LINK',`',
`define(AMP_1_LINK, `1')')

# if there is an external RT1308 amplifier connected over SoundWire,
# enable "EXT_AMP" option in the CMakefile.
ifdef(`EXT_AMP',
`
define(AMP_OFFSET, `1')
',
`
define(AMP_OFFSET, `0')
'
)

ifdef(`EXT_AMP_REF',
`
define(AMP_REF_OFFSET, `1')
',
`
define(AMP_REF_OFFSET, `0')
'
)

# Define pipeline id for intel-generic-dmic.m4
# to generate dmic setting
ifelse(CHANNELS, `0',
`
define(DMIC_OFFSET, `0')
'
,
`
define(DMIC_PCM_48k_ID, `10')
define(DMIC_PCM_16k_ID, `11')
define(DMIC_PIPELINE_48k_ID, `4')
define(DMIC_PIPELINE_16k_ID, `5')

define(DMIC_DAI_LINK_48k_ID, eval(JACK_OFFSET+AMP_OFFSET+AMP_REF_OFFSET))
define(DMIC_DAI_LINK_16k_ID, eval(JACK_OFFSET+AMP_OFFSET+AMP_REF_OFFSET+1))
include(`platform/intel/intel-generic-dmic.m4')

define(DMIC_OFFSET, `2')
'
)

define(HDMI_BE_ID_BASE, eval(JACK_OFFSET+AMP_OFFSET+AMP_REF_OFFSET+DMIC_OFFSET))

# Add Bluetooth Audio Offload pass-through

ifdef(`BT_OFFLOAD',
`	define(`BT_PIPELINE_PB_ID', `13')
	define(`BT_PIPELINE_CP_ID', `14')
	define(`BT_DAI_LINK_ID', eval(HDMI_BE_ID_BASE + 4))
	define(`BT_PCM_ID', `14') dnl use fixed PCM_ID
	define(HW_CONFIG_ID, eval(6 + DMIC_OFFSET))
	include(`platform/intel/intel-generic-bt.m4')
'
)

DEBUG_START

#
# Define the pipelines
#
# PCM0 ---> volume ----> mixer --->ALH 2 BE dailink 0
# PCM31 ---> volume ------^
# PCM1 <--- volume <---- ALH 3 BE dailink 1
ifdef(`EXT_AMP', `
# PCM2 ---> volume ----> ALH 2 BE dailink AMP_1_LINK
')
# PCM5 ---> volume <---- iDisp1
# PCM6 ---> volume <---- iDisp2
# PCM7 ---> volume <---- iDisp3
# PCM8 ---> volume <---- iDisp4
# PCM10 <----volume <---- DMIC01
# PCM11 <----volume <---- DMIC16k
ifdef(`BT_OFFLOAD',
`
# PCM14 <---> passthrough <---> SSP2 BT playback/capture
')

# Low Latency capture pipeline 2 on PCM 1 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-volume-switch-capture.m4,
	2, 1, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

ifdef(`EXT_AMP',
`
# Low Latency playback pipeline 3 on PCM 2 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	3, 2, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)
')
# Low Latency playback pipeline 6 on PCM 5 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	6, 5, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 7 on PCM 6 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	7, 6, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 8 on PCM 7 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	8, 7, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 9 on PCM 8 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
        9, 8, 2, s32le,
        1000, 0, 0,
        48000, 48000, 48000)

#
# DAIs configuration
#

dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     deadline, priority, core, time_domain)

# playback DAI is ALH(SDW0 PIN2) using 2 periods
# Buffers use s24le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-mixer-volume-dai-playback.m4,
	1, ALH, 2, SDW0-Playback,
	NOT_USED_IGNORED, 2, s24le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER, 2, 48000)

# capture DAI is ALH(SDW0 PIN2) using 2 periods
# Buffers use s24le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	2, ALH, 3, SDW0-Capture,
	PIPELINE_SINK_2, 2, s24le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# Low Latency playback pipeline 30 on PCM 0 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-host-volume-playback.m4,
	30, 0, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000,
	SCHEDULE_TIME_DOMAIN_TIMER,
	PIPELINE_PLAYBACK_SCHED_COMP_1)

# Deep buffer playback pipeline 31 on PCM 31 using max 2 channels of s32le
# Set 1000us deadline on core 0 with priority 0.
# TODO: Modify pipeline deadline to account for deep buffering
ifdef(`HEADSET_DEEP_BUFFER',
`
PIPELINE_PCM_ADD(sof/pipe-host-volume-playback.m4,
	31, 31, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000,
	SCHEDULE_TIME_DOMAIN_TIMER,
	PIPELINE_PLAYBACK_SCHED_COMP_1)
'
)

SectionGraph."mixer-host" {
	index "0"

	lines [
		# connect mixer dai pipelines to PCM pipelines
		dapm(PIPELINE_MIXER_1, PIPELINE_SOURCE_30)
ifdef(`HEADSET_DEEP_BUFFER',
`

		dapm(PIPELINE_MIXER_1, PIPELINE_SOURCE_31)
'
)
	]
}

ifdef(`EXT_AMP',
`
# playback DAI is ALH(AMP_1_LINK  PIN2) using 2 periods
# Buffers use s24le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	3, ALH,  eval(AMP_1_LINK * 256 + 2), `SDW'eval(AMP_1_LINK)`-Playback',
	PIPELINE_SOURCE_3, 2, s24le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)
')

# playback DAI is iDisp1 using 2 periods
# Buffers use s32le format, 1000us deadline with priority 0 on core 0
DAI_ADD(sof/pipe-dai-playback.m4,
	6, HDA, 0, iDisp1,
	PIPELINE_SOURCE_6, 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is iDisp2 using 2 periods
# Buffers use s32le format, 1000us deadline with priority 0 on core 0
DAI_ADD(sof/pipe-dai-playback.m4,
	7, HDA, 1, iDisp2,
	PIPELINE_SOURCE_7, 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is iDisp3 using 2 periods
# Buffers use s32le format, 1000us deadline with priority 0 on core 0
DAI_ADD(sof/pipe-dai-playback.m4,
	8, HDA, 2, iDisp3,
	PIPELINE_SOURCE_8, 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is iDisp4 using 2 periods
# Buffers use s32le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
        9, HDA, 3, iDisp4,
        PIPELINE_SOURCE_9, 2, s32le,
        1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# PCM Low Latency, id 0
dnl PCM_PLAYBACK_ADD(name, pcm_id, playback)
PCM_PLAYBACK_ADD(Jack Out, 0, PIPELINE_PCM_30)
ifdef(`HEADSET_DEEP_BUFFER',
`
PCM_PLAYBACK_ADD(Jack Out DeepBuffer, 31, PIPELINE_PCM_31)
'
)
PCM_CAPTURE_ADD(Jack In, 1, PIPELINE_PCM_2)
ifdef(`EXT_AMP',
`
PCM_PLAYBACK_ADD(Speaker, 2, PIPELINE_PCM_3)
')
PCM_PLAYBACK_ADD(HDMI 1, 5, PIPELINE_PCM_6)
PCM_PLAYBACK_ADD(HDMI 2, 6, PIPELINE_PCM_7)
PCM_PLAYBACK_ADD(HDMI 3, 7, PIPELINE_PCM_8)
PCM_PLAYBACK_ADD(HDMI 4, 8, PIPELINE_PCM_9)
#
# BE configurations - overrides config in ACPI if present
#

#ALH dai index = ((link_id << 8) | PDI id)
#ALH SDW0 Pin2 (ID: 0)
DAI_CONFIG(ALH, 2, 0, SDW0-Playback,
	ALH_CONFIG(ALH_CONFIG_DATA(ALH, 2, 48000, 2)))

#ALH SDW0 Pin3 (ID: 1)
DAI_CONFIG(ALH, 3, 1, SDW0-Capture,
	ALH_CONFIG(ALH_CONFIG_DATA(ALH, 3, 48000, 2)))

ifdef(`EXT_AMP',
`
#ALH SDW AMP_1_LINK Pin2 (ID: 2)
DAI_CONFIG(ALH, eval(AMP_1_LINK * 256 + 2), 2, `SDW'eval(AMP_1_LINK)`-Playback',
	ALH_CONFIG(ALH_CONFIG_DATA(ALH, eval(AMP_1_LINK * 256 + 2), 48000, 2)))
')

# 3 HDMI/DP outputs
DAI_CONFIG(HDA, 0, HDMI_BE_ID_BASE, iDisp1,
	HDA_CONFIG(HDA_CONFIG_DATA(HDA, 0, 48000, 2)))
DAI_CONFIG(HDA, 1, eval(HDMI_BE_ID_BASE + 1), iDisp2,
	HDA_CONFIG(HDA_CONFIG_DATA(HDA, 1, 48000, 2)))
DAI_CONFIG(HDA, 2, eval(HDMI_BE_ID_BASE + 2), iDisp3,
	HDA_CONFIG(HDA_CONFIG_DATA(HDA, 2, 48000, 2)))
DAI_CONFIG(HDA, 3, eval(HDMI_BE_ID_BASE + 3), iDisp4,
	HDA_CONFIG(HDA_CONFIG_DATA(HDA, 3, 48000, 2)))

DEBUG_END
