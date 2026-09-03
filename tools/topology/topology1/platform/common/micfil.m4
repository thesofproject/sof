divert(-1)

dnl MICFIL related macros

define(`MICFIL_CONFIG',
`}'
$1
)
dnl MICFIL_CONFIG_DATA(type, idx, rate, channel)
define(`MICFIL_CONFIG_DATA',
`SectionVendorTuples."'N_DAI_CONFIG($1$2)`_tuples" {'
`       tokens "sof_micfil_tokens"'
`       tuples."word" {'
`               SOF_TKN_IMX_MICFIL_RATE'	STR($3)
`		SOF_TKN_IMX_MICFIL_CH'		STR($4)
`       }'
`}'
`SectionData."'N_DAI_CONFIG($1$2)`_data" {'
`       tuples "'N_DAI_CONFIG($1$2)`_tuples"'
`}'
)

divert(0)dnl

