divert(-1)

dnl AFE related macros

dnl AFE_CONFIG(afe_config_data)
define(`AFE_CONFIG',
`}'
$1
)

dnl AFE_CONFIG_DATA(type, idx, rate, channels, format)
define(`AFE_CONFIG_DATA',
`SectionVendorTuples."'N_DAI_CONFIG($1$2)`_tuples" {'
`	tokens "sof_afe_tokens"'
`	tuples."word" {'
`		SOF_TKN_MEDIATEK_AFE_RATE'		STR($3)
`		SOF_TKN_MEDIATEK_AFE_CH'		STR($4)
`	}'
`	tuples."string" {'
`		SOF_TKN_MEDIATEK_AFE_FORMAT'		STR($5)
`	}'
`}'
`SectionData."'N_DAI_CONFIG($1$2)`_data" {'
`	tuples "'N_DAI_CONFIG($1$2)`_tuples"'
`}'
)

divert(0)dnl
