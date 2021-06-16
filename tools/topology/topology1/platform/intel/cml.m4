include(`platform/intel/cnl.m4')
include(`abi.h')

#SSP setting for CML platform
undefine(`SSP_INDEX')
define(`SSP_INDEX', 0)

undefine(`SSP_NAME')
define(`SSP_NAME', `SSP0-Codec')

undefine(`SSP_MCLK_RATE')
define(`SSP_MCLK_RATE', `24000000')


#SSP1 setting for CML platform
define(`SSP1_INDEX', 1)
define(`SSP1_NAME', `SSP1-Codec')
define(`SSP1_MCLK_RATE', `24000000')

ifelse(SOF_ABI_VERSION_3_9_OR_GRT, `1',
`
# --- ABI VERSION is 3.9 or higher case ----
# Low Latency playback pipeline 8 on PCM 6 using max 2 channels of s32le.
# Schedule 1000us deadline on core 0 with priority 0
# pipe-src pipeline is needed for older SSP config
define(`PIPE_VOLUME_PLAYBACK', `sof/pipe-volume-playback.m4')
define(`SSP1_VALID_BITS', `24')

# playback DAI is SSP1 using 2 periods
# Buffers use s16le format, with 48 frame per 1000us on core 0 with priority 0
# With m/n divider available we can support 24 bit playback
define(`SSP1_VALID_BITS_STR', `s24le')

# Note: sof-cml-rt5682-max98357.m4 has variable bclk/fsync
#       sof-cml-rt1011-rt5682.m4 has fixed bclk/fsync
define(`SSP1_BCLK', `2304000')
define(`SSP1_FSYNC', `48000')

#SSP 1 (ID: 6)
#Use BCLK delay in SSP_CONFIG_DATA only on supporting version
# Note: this is for sof-cml-rt1011-rt5682.m4
# for sof-cml-rt5682-max98357.m4 uses 3 parameters and bits per samples set by SSP1_VALID_BITS
define(`SET_SSP1_CONFIG_DATA', `SSP, 1, 24, 0, 0, 10')',
`
# --- ABI VERSION is below 3.9 case ----
# Low Latency playback pipeline 8 on PCM 6 using max 2 channels of s32le.
# Schedule 1000us deadline on core 0 with priority 0
# pipe-src pipeline is needed for older SSP config
define(`PIPE_VOLUME_PLAYBACK', `sof/pipe-src-volume-playback.m4')
define(`SSP1_VALID_BITS', `16')

# playback DAI is SSP1 using 2 periods
# Buffers use s16le format, with 48 frame per 1000us on core 0 with priority 0
# With m/n divider available we can support 24 bit playback
define(`SSP1_VALID_BITS_STR', `s16le')

# Note: sof-cml-rt5682-max98357.m4 has variable bclk/fsync
#       sof-cml-rt1011-rt5682.m4 has fixed bclk/fsync
define(`SSP1_BCLK', `1500000')
define(`SSP1_FSYNC', `46875')

#SSP 1 (ID: 6)
#Use BCLK delay in SSP_CONFIG_DATA only on supporting version
# Note: this is for sof-cml-rt1011-rt5682.m4
# for sof-cml-rt5682-max98357.m4 uses 3 parameters and bits per samples set by SSP1_VALID_BITS
define(`SET_SSP1_CONFIG_DATA', `SSP, 1, 24')')

include(`platform/intel/dmic.m4')

# Note: this is for sof-cml-da7219-max98357a.m4
# include the SSP1 settings and common SSP settings additionally

define(`DMIC_PIPE_CAPTURE', `sof/pipe-passthrough-capture.m4')

undefine(`DMIC01_FMT')
define(`DMIC01_FMT', `s32le')

undefine(`DMIC1_FMT')
define(`DMIC1_FMT', `s24le')

undefine(`HDMI0_INDEX')
define(`HDMI0_INDEX', `0')

undefine(`HDMI1_INDEX')
define(`HDMI1_INDEX', `1')

undefine(`HDMI2_INDEX')
define(`HDMI2_INDEX', `2')

undefine(`SSP_BCLK')
define(`SSP_BCLK', `2400000')

undefine(`SSP_FSYNC', `48000')
define(`SSP_FSYNC', `48000')

undefine(`SSP_BITS_WIDTH')
define(`SSP_BITS_WIDTH', `25')

undefine(`SSP_VALID_BITS')
define(`SSP_VALID_BITS', `16')

undefine(`MCLK_ID')
define(`MCLK_ID', `')
