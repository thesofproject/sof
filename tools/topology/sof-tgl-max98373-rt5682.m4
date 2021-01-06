#
# Topology for Tigerlake with Max98373 amp (SSP: $AMP_SSP)  + rt5682 codec + DMIC + 4 HDMI
#

# Include topology builder
include(`utils.m4')
include(`dai.m4')
include(`pipeline.m4')
include(`ssp.m4')
include(`hda.m4')

# Include TLV library
include(`common/tlv.m4')

# Include Token library
include(`sof/tokens.m4')

# Include Tigerlake DSP configuration
include(`platform/intel/tgl.m4')
include(`platform/intel/dmic.m4')

DEBUG_START

#
# Define the pipelines
#
# PCM0 ----> smart_amp ----> SSP$AMP_SSP (Speaker -max98373)
#              ^
#              |
#              |
# PCM0 <---- demux <----- SSP$AMP_SSP (Speaker -max98373)
# PCM1 <---> volume <----> SSP0  (Headset - ALC5682)
# PCM2 ----> volume -----> iDisp1
# PCM3 ----> volume -----> iDisp2
# PCM4 ----> volume -----> iDisp3
# PCM5 ----> volume -----> iDisp4
# PCM99 <---- volume <---- DMIC01 (dmic 48k capture)
# PCM100 <---- kpb <---- DMIC16K (dmic 16k capture)


ifdef(`AMP_SSP',`',`errprint(note: Define AMP_SSP for speaker amp SSP Index)')
# Smart amplifier related
# SSP related
#define smart amplifier SSP index
define(`SMART_SSP_INDEX', AMP_SSP)
#define SSP BE dai_link name
define(`SMART_SSP_NAME', concat(concat(`SSP', AMP_SSP),`-Codec'))
#define BE dai_link ID
define(`SMART_BE_ID', 7)
#define SSP mclk
define(`SSP_MCLK', 24576000)
# Playback related
define(`SMART_PB_PPL_ID', 1)
define(`SMART_PB_CH_NUM', 2)
define(`SMART_TX_CHANNELS', 4)
define(`SMART_RX_CHANNELS', 4)
define(`SMART_FB_CHANNELS', 4)
# Ref capture related
define(`SMART_REF_PPL_ID', 11)
define(`SMART_REF_CH_NUM', 2)
# PCM related
define(`SMART_PCM_ID', 0)
define(`SMART_PCM_NAME', `smart373-spk')

# UUID related
DECLARE_SOF_RT_UUID("Maxim DSM", maxim_dsm_comp_uuid, 0x0cd84e80, 0xebd3,
                    0x11ea, 0xad, 0xc1, 0x02, 0x42, 0xac, 0x12, 0x00, 0x02);
define(`SMART_UUID', maxim_dsm_comp_uuid)
# Include Smart Amplifier support
include(`sof-smart-amplifier.m4')

# Define pipeline id for intel-generic-dmic-kwd.m4
# to generate dmic setting with kwd when we have dmic
# define channel
define(CHANNELS, `4')
# define kfbm with volume
define(KFBM_TYPE, `vol-kfbm')
# define pcm, pipeline and dai id
define(DMIC_PCM_48k_ID, `99')
define(DMIC_PIPELINE_48k_ID, `4')
define(DMIC_DAI_LINK_48k_ID, `1')
define(DMIC_PCM_16k_ID, `100')
define(DMIC_PIPELINE_16k_ID, `9')
define(DMIC_PIPELINE_KWD_ID, `10')
define(DMIC_DAI_LINK_16k_ID, `2')
# define pcm, pipeline and dai id
define(KWD_PIPE_SCH_DEADLINE_US, 20000)
# include the generic dmic with kwd
include(`platform/intel/intel-generic-dmic-kwd.m4')

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     frames, deadline, priority, core)

# Low Latency playback pipeline 2 on PCM 1 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
        2, 1, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency capture pipeline 3 on PCM 1 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-capture.m4,
        3, 1, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 5 on PCM 2 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	5, 2, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 6 on PCM 3 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	6, 3, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 7 on PCM 4 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	7, 4, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 8 on PCM 5 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	8, 5, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

#
# DAIs configuration
#

dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     frames, deadline, priority, core)

# playback DAI is SSP0 using 2 periods
# Buffers use s24le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
        2, SSP, 0, SSP0-Codec,
        PIPELINE_SOURCE_2, 2, s32le,
        1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# capture DAI is SSP0 using 2 periods
# Buffers use s24le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
        3, SSP, 0, SSP0-Codec,
        PIPELINE_SINK_3, 2, s32le,
        1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is iDisp1 using 2 periods
# Buffers use s32le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	5, HDA, 0, iDisp1,
	PIPELINE_SOURCE_5, 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is iDisp2 using 2 periods
# Buffers use s32le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	6, HDA, 1, iDisp2,
	PIPELINE_SOURCE_6, 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is iDisp3 using 2 periods
# Buffers use s32le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	7, HDA, 2, iDisp3,
	PIPELINE_SOURCE_7, 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is iDisp4 using 2 periods
# Buffers use s32le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	8, HDA, 3, iDisp4,
	PIPELINE_SOURCE_8, 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

#
# Bind PCM with the pipeline
#
dnl PCM_PLAYBACK_ADD(name, pcm_id, playback)
PCM_DUPLEX_ADD(Headset, 1, PIPELINE_PCM_2, PIPELINE_PCM_3)
PCM_PLAYBACK_ADD(HDMI1, 2, PIPELINE_PCM_5)
PCM_PLAYBACK_ADD(HDMI2, 3, PIPELINE_PCM_6)
PCM_PLAYBACK_ADD(HDMI3, 4, PIPELINE_PCM_7)
PCM_PLAYBACK_ADD(HDMI4, 5, PIPELINE_PCM_8)

#
# BE configurations - overrides config in ACPI if present
#
dnl DAI_CONFIG(type, dai_index, link_id, name, ssp_config/dmic_config)
dnl SSP_CONFIG(format, mclk, bclk, fsync, tdm, ssp_config_data)
dnl SSP_CLOCK(clock, freq, codec_master, polarity)
dnl SSP_CONFIG_DATA(type, idx, valid bits, mclk_id)
dnl mclk_id is optional
dnl ssp1-maxmspk, ssp0-RTHeadset

#SSP 0 (ID: 0)
DAI_CONFIG(SSP, 0, 0, SSP0-Codec,
        SSP_CONFIG(I2S, SSP_CLOCK(mclk, SSP_MCLK, codec_mclk_in),
                      SSP_CLOCK(bclk, 3072000, codec_slave),
                      SSP_CLOCK(fsync, 48000, codec_slave),
                      SSP_TDM(2, 32, 3, 3),
                      SSP_CONFIG_DATA(SSP, 0, 32)))

# 4 HDMI/DP outputs (ID: 3,4,5,6)
DAI_CONFIG(HDA, 0, 3, iDisp1,
	HDA_CONFIG(HDA_CONFIG_DATA(HDA, 0, 48000, 2)))
DAI_CONFIG(HDA, 1, 4, iDisp2,
	HDA_CONFIG(HDA_CONFIG_DATA(HDA, 1, 48000, 2)))
DAI_CONFIG(HDA, 2, 5, iDisp3,
	HDA_CONFIG(HDA_CONFIG_DATA(HDA, 2, 48000, 2)))
DAI_CONFIG(HDA, 3, 6, iDisp4,
	HDA_CONFIG(HDA_CONFIG_DATA(HDA, 3, 48000, 2)))

DEBUG_END
