divert(-1)

dnl MULTI DAI related macros

dnl MULTI_DAI_CONFIG(type, index1, index2, index3, index4)
define(`MULTI_DAI_CONFIG',
`}'
$1
)

dnl MULTI_DAI_DATA(type, idx, subdai type, subdai index 0, subdai index 1, subdai index 2, subdai index 3)
define(`MULTI_DAI_DATA',
`SectionVendorTuples."'N_DAI_CONFIG($1$2)`_tuples" {'
`	tokens "sof_multi_dai_tokens"'
`	tuples."string" {'
`		SOF_TKN_MULTIDAI_SUBTYPE'		STR($3)
`	}'
`	tuples."word" {'
`		SOF_TKN_MULTIDAI_INDEX0'			STR($4)
`		SOF_TKN_MULTIDAI_INDEX1'			STR($5)
`		SOF_TKN_MULTIDAI_INDEX2'			STR($6)
`		SOF_TKN_MULTIDAI_INDEX3'			STR($7)
`	}'
`}'
`SectionData."'N_DAI_CONFIG($1$2)`_data" {'
`	tuples "'N_DAI_CONFIG($1$2)`_tuples"'
`}'
)

divert(0)dnl
