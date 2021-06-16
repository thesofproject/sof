divert(-1)

dnl HDA related macros

dnl HDA_CONFIG(hda_config_data)
define(`HDA_CONFIG',
`}'
$1
)

dnl HDA_CONFIG_DATA(type, idx, rate, channels)
define(`HDA_CONFIG_DATA',
`SectionVendorTuples."'N_DAI_CONFIG($1$2)`_tuples" {'
`	tokens "sof_hda_tokens"'
`	tuples."word" {'
`		SOF_TKN_INTEL_HDA_RATE'		STR($3)
`		SOF_TKN_INTEL_HDA_CH'		STR($4)
`	}'
`}'
`SectionData."'N_DAI_CONFIG($1$2)`_data" {'
`	tuples "'N_DAI_CONFIG($1$2)`_tuples"'
`}'
)

divert(0)dnl
