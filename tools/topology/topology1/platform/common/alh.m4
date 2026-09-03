divert(-1)

dnl ALH related macros

dnl ALH_CONFIG(alh_config_data)
define(`ALH_CONFIG',
`}'
$1
)

dnl ALH_CONFIG_DATA(type, idx, rate, channels)
define(`ALH_CONFIG_DATA',
`SectionVendorTuples."'N_DAI_CONFIG($1$2)`_tuples" {'
`	tokens "sof_alh_tokens"'
`	tuples."word" {'
`		SOF_TKN_INTEL_ALH_RATE'		STR($3)
`		SOF_TKN_INTEL_ALH_CH'		STR($4)
`	}'
`}'
`SectionData."'N_DAI_CONFIG($1$2)`_data" {'
`	tuples "'N_DAI_CONFIG($1$2)`_tuples"'
`}'
)

divert(0)dnl
