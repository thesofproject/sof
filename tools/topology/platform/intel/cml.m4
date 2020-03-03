include(`platform/intel/cnl.m4')

#SSP setting for CML platform
undefine(`SSP_INDEX')
define(`SSP_INDEX', 0)

undefine(`SSP_NAME')
define(`SSP_NAME', `SSP0-Codec')

undefine(`SSP_MCLK_RATE')
define(`SSP_MCLK_RATE', `24000000')

include(`platform/intel/dmic.m4')
