#
# Topology for SKL+ HDA Generic machine w/ iDISP codec only
#

# Include topology builder
include(`utils.m4')
include(`dai.m4')
include(`pipeline.m4')

# Include TLV library
include(`common/tlv.m4')

# Include Token library
include(`sof/tokens.m4')

# Include bxt DSP configuration
include(`platform/intel/bxt.m4')

# Define pipeline id for intel-generic-dmic.m4
# to generate dmic setting

ifelse(CHANNELS, `0', ,
`
define(DMIC_PIPELINE_48k_ID, `5')
define(DMIC_PIPELINE_16k_ID, `6')

include(`platform/intel/intel-generic-dmic.m4')
'
)

#
# Define the pipelines
#
# PCM1 ----> volume -----> iDisp1
# PCM2 ----> volume -----> iDisp2
# PCM3 ----> volume -----> iDisp3
#

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate,
dnl     time_domain, sched_comp)

# Low Latency playback pipeline 7 on PCM 3 using max 2 channels of s24le.
# 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	7, 3, 2, s24le,
        1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 8 on PCM 4 using max 2 channels of s24le.
# 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
        8, 4, 2, s24le,
        1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 9 on PCM 5 using max 2 channels of s24le.
# 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
        9, 5, 2, s24le,
        1000, 0, 0,
	48000, 48000, 48000)

#
# DAIs configuration
#

dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     deadline, priority, core)
# playback DAI is iDisp1 using 2 periods
# Dai buffers use s32le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
        7, HDA, 4, iDisp1,
        PIPELINE_SOURCE_7, 2, s32le,
        1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is iDisp2 using 2 periods
# Dai buffers use s32le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
        8, HDA, 5, iDisp2,
        PIPELINE_SOURCE_8, 2, s32le,
        1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is iDisp3 using 2 periods
# Dai buffers use s32le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
        9, HDA, 6, iDisp3,
        PIPELINE_SOURCE_9, 2, s32le,
        1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

#
# PCM Low Latency, id 0
dnl PCM_PLAYBACK_ADD(name, pcm_id, playback)

PCM_PLAYBACK_ADD(HDMI1, 3, PIPELINE_PCM_7)
PCM_PLAYBACK_ADD(HDMI2, 4, PIPELINE_PCM_8)
PCM_PLAYBACK_ADD(HDMI3, 5, PIPELINE_PCM_9)

#
# BE configurations - overrides config in ACPI if present
#


# 3 HDMI/DP outputs (ID: 3,4,5)
DAI_CONFIG(HDA, 4, 1, iDisp1)
DAI_CONFIG(HDA, 5, 2, iDisp2)
DAI_CONFIG(HDA, 6, 3, iDisp3)

VIRTUAL_DAPM_ROUTE_OUT(iDisp1_out, HDA, 4, OUT, 7)
VIRTUAL_DAPM_ROUTE_OUT(iDisp2_out, HDA, 5, OUT, 8)
VIRTUAL_DAPM_ROUTE_OUT(iDisp3_out, HDA, 6, OUT, 9)

VIRTUAL_WIDGET(iDisp3 Tx, out_drv, 0)
VIRTUAL_WIDGET(iDisp2 Tx, out_drv, 1)
VIRTUAL_WIDGET(iDisp1 Tx, out_drv, 2)
