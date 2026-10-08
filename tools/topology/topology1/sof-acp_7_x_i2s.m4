#
#  Topology for ACP_7_X with TDM.
#
# Include topology builder
include(`utils.m4')
include(`dai.m4')
include(`pipeline.m4')
include(`acp-tdm.m4')

# Include TLV library
include(`common/tlv.m4')

# Include Token library
include(`sof/tokens.m4')

# Include ACP DSP configuration
include(`platform/amd/acp.m4')

#
#  Pipeline Graph (16-bit / s16le):
#
#  PLAYBACK:
#  [Host PCM ]  ->  [Passthrough Pipeline ]  ->  [ACPTDM DAI ]
#   s16le 2ch            s16le 48kHz               s16le 2ch
#                               |
#                        [acp-i2s0-codec]
#                        I2S bclk=3.072MHz
#                        fsync=48kHz, 2ch
#                               |
#  CAPTURE:
#  [ACPTDM DAI ]  ->  [Passthrough Pipeline ]  ->  [Host PCM ]
#   s16le 2ch              s16le 48kHz               s16le 2ch
#

DEBUG_START
#======================================================================
# Playback pipeline 1 on PCM 0 using max 2 channels of s16le.

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate)
# Playback pipeline 0 on PCM 0, dai index 0, link id 0 using max 2 channels of s16le.

# Schedule 96 frames per 2000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-passthrough-playback.m4,
        0, 0, 2, s16le,
        2000, 0, 0,
        48000, 48000, 48000)

# Capture pipeline 3 on PCM 0, dai index 0 using max 2 channels of s16le.
PIPELINE_PCM_ADD(sof/pipe-passthrough-capture.m4,
        3, 0, 2, s16le,
        2000, 0, 0,
        48000, 48000, 48000)
#===========================================================================
dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     deadline, priority, core, time_domain)

# Schedule 96 frames per 2000us deadline on core 0 with priority 0

# playback DAI is ACPTDM using 2 periods
DAI_ADD(sof/pipe-dai-playback.m4,
        0, ACPTDM, 0, acp-i2s0-codec,
        PIPELINE_SOURCE_0, 2, s16le,
        2000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# Capture DAI is ACPTDM using 2 periods
DAI_ADD(sof/pipe-dai-capture.m4,
        3, ACPTDM, 0, acp-i2s0-codec,
        PIPELINE_SINK_3, 2, s16le,
        2000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)
#===========================================================================
# playback DAI is ACPTDM using 2 periods

dnl DAI_CONFIG(type, dai_index, link_id, name, ACPTDM_config/acpdmic_config)
dnl ACPTDM_CONFIG(format, mclk, bclk, fsync, tdm, ACPTDM_config_data)
dnl ACP_CLOCK(clock, freq, codec_provider, polarity)
dnl ACPTDM_CONFIG_DATA(type, idx, valid bits, mclk_id)
dnl mclk_id is optional

DAI_CONFIG(ACPTDM, 0, 0, acp-i2s0-codec,
           ACPTDM_CONFIG(I2S, ACP_CLOCK(mclk, 49152000, codec_mclk_in),
                ACP_CLOCK(bclk, 3072000, codec_consumer),
                ACP_CLOCK(fsync, 48000, codec_consumer),
                ACP_TDM(2, 32, 3, 3),ACPTDM_CONFIG_DATA(ACPTDM, 0, 48000, 2, 0, 0))
)
dnl PCM_DUPLEX_ADD(name, pcm_id, playback_pipeline, capture_pipeline)
PCM_DUPLEX_ADD(I2STDM0, 0, PIPELINE_PCM_0, PIPELINE_PCM_3)

#====================================================================================================================
# TDM instance 1

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate)

PIPELINE_PCM_ADD(sof/pipe-passthrough-playback.m4,
        1, 1, 2, s16le,
        2000, 0, 0,
        48000, 48000, 48000)
PIPELINE_PCM_ADD(sof/pipe-passthrough-capture.m4,
        4, 1, 2, s16le,
        2000, 0, 0,
        48000, 48000, 48000)

dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     deadline, priority, core, time_domain)

DAI_ADD(sof/pipe-dai-playback.m4,
        1, ACPTDM, 1, acp-i2s1-codec,
        PIPELINE_SOURCE_1, 2, s16le,
        2000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

DAI_ADD(sof/pipe-dai-capture.m4,
        4, ACPTDM, 1, acp-i2s1-codec,
        PIPELINE_SINK_4, 2, s16le,
        2000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)
dnl DAI_CONFIG(type, dai_index, link_id, name, ACPTDM_config/acpdmic_config)
dnl ACPTDM_CONFIG(format, mclk, bclk, fsync, tdm, ACPTDM_config_data)
dnl ACP_CLOCK(clock, freq, codec_provider, polarity)
dnl ACPTDM_CONFIG_DATA(type, idx, valid bits, mclk_id)
dnl mclk_id is optional

DAI_CONFIG(ACPTDM, 1, 1, acp-i2s1-codec,
           ACPTDM_CONFIG(I2S, ACP_CLOCK(mclk, 49152000, codec_mclk_in),
                ACP_CLOCK(bclk, 3072000, codec_consumer),
                ACP_CLOCK(fsync, 48000, codec_consumer),
                ACP_TDM(2, 32, 3, 3), ACPTDM_CONFIG_DATA(ACPTDM, 1, 48000, 2, 0)))

dnl PCM_DUPLEX_ADD(name, pcm_id, playback_pipeline, capture_pipeline)
PCM_DUPLEX_ADD(I2STDM1, 1, PIPELINE_PCM_1, PIPELINE_PCM_4)

#====================================================================================================================
# TDM instance 2 -- speaker path through smart_amp (2x max98388)
#
#  playback:  host PCM 2 --B0--> smart_amp --B1--> ACPTDM2 TX
#                                   ^
#                                   B2 (IV feedback)
#                                   |
#  capture :  ACPTDM2 RX --B1--> demux --B0--> host PCM 2 (echo reference)
#

dnl Macros required by both smart_amp pipe files; either missing triggers fatal_error.
dnl Values are for a 2-channel configuration, consistent with the
dnl ACP_TDM(2, 32, 3, 3) (2 slots) used by this link.
dnl SMART_PB_PPL_ID  : playback pipeline id
dnl SMART_REF_PPL_ID : echo reference capture pipeline id
dnl SMART_PB_CH_NUM  : playback channel count
dnl SMART_TX_CHANNELS/SMART_RX_CHANNELS : DAI TX/RX channel count
dnl SMART_FB_CHANNELS: feedback (IV) channel count
dnl SMART_REF_CH_NUM : echo reference channel count
define(`SMART_PB_PPL_ID', 2)
define(`SMART_REF_PPL_ID', 5)
define(`SMART_PB_CH_NUM', 2)
define(`SMART_TX_CHANNELS', 2)
define(`SMART_RX_CHANNELS', 2)
define(`SMART_FB_CHANNELS', 2)
define(`SMART_REF_CH_NUM', 2)

dnl ACP7x TDM2 has only 2 slots; each max98388 occupies one slot and
dnl interleaves V and I within that slot (ivFormat=3, MSB flag).  The echo
dnl reference second channel must therefore come from in ch1 (slot 1, second
dnl amp), not from the pipe default in ch2 (which is the slot-2 position in a
dnl 4-slot layout, does not exist in a 2-channel stream, and is silently
dnl discarded by mux.c:215, permanently silencing ch1).
define(`REF_CHMAP', `0x01,0x02,0x00,0x00,0x00,0x00,0x00,0x00')

dnl smart_amp.c registers one of two UUIDs depending on CONFIG_MAXIM_DSM:
dnl   CONFIG_MAXIM_DSM=y  -> maxim_dsm           0cd84e80-ebd3-11ea-adc1-0242ac120002
dnl   otherwise (PASSTHRU_AMP) -> passthru_smart_amp 64a794f0-55d3-4bca-9d5b-7b588badd037
dnl A mismatch between the topology and the firmware causes comp_new to fail
dnl with -EINVAL (driver not found), so this must be updated whenever the
dnl Kconfig is switched.
dnl SMART_UUID must be defined before PIPELINE_PCM_ADD: the playback pipe
dnl tests `ifdef(`SMART_UUID',...)`.  Defining it explicitly here (rather
dnl than relying on the pipe's own default) makes the current build mode
dnl visible at a glance in this file.
DECLARE_SOF_RT_UUID("maxim_dsm", maxim_dsm_smart_amp_uuid, 0x0cd84e80, 0xebd3, 0x11ea, 0xad, 0xc1, 0x02, 0x42, 0xac, 0x12, 0x00, 0x02)
define(`SMART_UUID', maxim_dsm_smart_amp_uuid)
dnl Passthru variant (use when CONFIG_MAXIM_DSM is not set):
dnl   DECLARE_SOF_RT_UUID("passthru_smart_amp", passthru_smart_amp_uuid, 0x64a794f0, 0x55d3, 0x4bca, 0x9d, 0x5b, 0x7b, 0x58, 0x8b, 0xad, 0xd0, 0x37)
dnl   define(`SMART_UUID', passthru_smart_amp_uuid)

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate)

dnl Playback uses the upstream pipe; SMART_UUID defined above selects the Maxim
dnl DSM build of smart_amp.
PIPELINE_PCM_ADD(sof/pipe-smart-amplifier-playback.m4,
        2, 2, 2, s16le,
        2000, 0, 0,
        48000, 48000, 48000)

dnl Capture uses the upstream original (the reference project has no AMD-specific version).
PIPELINE_PCM_ADD(sof/pipe-amp-ref-capture.m4,
        5, 2, 2, s16le,
        2000, 0, 0,
        48000, 48000, 48000)

dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     deadline, priority, core, time_domain)

dnl Keep SCHEDULE_TIME_DOMAIN_TIMER as used by the existing ACP7x platform.
dnl The reference project uses DMA scheduling, which is an older ACPHS
dnl platform difference that does not apply here.
DAI_ADD(sof/pipe-dai-playback.m4,
        2, ACPTDM, 2, acp-i2s2-codec,
        PIPELINE_SOURCE_2, 2, s16le,
        2000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

DAI_ADD(sof/pipe-dai-capture.m4,
        5, ACPTDM, 2, acp-i2s2-codec,
        PIPELINE_SINK_5, 2, s16le,
        2000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

dnl DAI_CONFIG(type, dai_index, link_id, name, ACPTDM_config/acpdmic_config)
dnl ACPTDM_CONFIG(format, mclk, bclk, fsync, tdm, ACPTDM_config_data)
dnl ACP_CLOCK(clock, freq, codec_provider, polarity)
dnl ACPTDM_CONFIG_DATA(type, idx, valid bits, mclk_id)
dnl mclk_id is optional

DAI_CONFIG(ACPTDM, 2, 2, acp-i2s2-codec,
           ACPTDM_CONFIG(I2S, ACP_CLOCK(mclk, 49152000, codec_mclk_in),
                ACP_CLOCK(bclk, 3072000, codec_consumer),
                ACP_CLOCK(fsync, 48000, codec_consumer),
                ACP_TDM(2, 32, 3, 3), ACPTDM_CONFIG_DATA(ACPTDM, 2, 48000, 2, 0)))

dnl Wire the capture pipeline's demux output into the smart_amp feedback buffer
dnl in the playback pipeline.  This cross-pipeline connection cannot be
dnl expressed in a single pipe file and must be added here.
dnl N_SMART_REF_BUF is defined by the playback pipe (BUF2.2);
dnl N_SMART_DEMUX is defined by the capture pipe.
ifdef(`N_SMART_REF_BUF',`',`fatal_error(note: N_SMART_REF_BUF undefined - playback pipe missing
)')
ifdef(`N_SMART_DEMUX',`',`fatal_error(note: N_SMART_DEMUX undefined - ref capture pipe missing
)')
SectionGraph."PIPE_SMART_AMP" {
	index "0"

	lines [
		# demux -> smart_amp feedback
		dapm(N_SMART_REF_BUF, N_SMART_DEMUX)
	]
}

dnl PCM_DUPLEX_ADD(name, pcm_id, playback_pipeline, capture_pipeline)
dnl Keep the name I2STDM2 to avoid disturbing host-side UCM / existing test commands.
PCM_DUPLEX_ADD(I2STDM2, 2, PIPELINE_PCM_2, PIPELINE_PCM_5)

dnl Clean up macros introduced by this section.
undefine(`SMART_PB_PPL_ID')
undefine(`SMART_REF_PPL_ID')
undefine(`SMART_PB_CH_NUM')
undefine(`SMART_TX_CHANNELS')
undefine(`SMART_RX_CHANNELS')
undefine(`SMART_FB_CHANNELS')
undefine(`SMART_REF_CH_NUM')
undefine(`REF_CHMAP')
undefine(`SMART_UUID')
#====================================================================================================================

DEBUG_END
