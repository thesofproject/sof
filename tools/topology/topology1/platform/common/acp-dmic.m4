divert(-1)

dnl ACPDMIC related macros

define(`ACPDMIC_CONFIG',
`}'
$1
)
dnl ACPDMIC_CONFIG_DATA(type, idx, rate, channel)
define(`ACPDMIC_CONFIG_DATA',
`SectionVendorTuples."'N_DAI_CONFIG($1$2)`_tuples" {'
`       tokens "sof_acpdmic_tokens"'
`       tuples."word" {'
`               SOF_TKN_AMD_ACPDMIC_RATE'	STR($3)
`		SOF_TKN_AMD_ACPDMIC_CH'		STR($4)
`       }'
`}'
`SectionData."'N_DAI_CONFIG($1$2)`_data" {'
`       tuples "'N_DAI_CONFIG($1$2)`_tuples"'
`}'
)

divert(0)dnl

