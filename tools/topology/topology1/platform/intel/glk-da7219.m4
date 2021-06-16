include(`platform/intel/bxt.m4')

# SSP/DMIC machine specific settings on GLK platform
undefine(`SSP_INDEX')
define(`SSP_INDEX', 2)

undefine(`SSP_NAME')
define(`SSP_NAME', `SSP2-Codec')

undefine(`DMIC_PCM_NUM')
define(`DMIC_PCM_NUM', `99')

undefine(`UNUSED_SSP_ROUTE1')
define(`UNUSED_SSP_ROUTE1', `ssp1')

undefine(`UNUSED_SSP_ROUTE2')
define(`UNUSED_SSP_ROUTE2', `ssp2')
