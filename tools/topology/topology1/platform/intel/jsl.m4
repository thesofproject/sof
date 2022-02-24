#
# Jasperlake differentiation for pipelines and components
#

include(`platform/intel/icl.m4')

define(`SPK_INDEX', `1')
define(`SPK_NAME', `SSP1-Codec')

undefine(`SPK_DATA_FORMAT')
define(`SPK_DATA_FORMAT', `s16le')

define(`SET_SSP_CONFIG',
                `SSP_CONFIG(DSP_B, SSP_CLOCK(mclk, 24000000, codec_mclk_in),
                        SSP_CLOCK(bclk, 4800000, codec_slave),
                        SSP_CLOCK(fsync, 48000, codec_slave),
                        SSP_TDM(4, 25, 3, 240),
                        SSP_CONFIG_DATA(SSP, SPK_INDEX, 16))'
      )
