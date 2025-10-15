divert(-1)

dnl ACP_SDW related macros

dnl ACP_CONFIG(format, mclk, bclk, fsync, tdm, sdw_config_data)
define(`ACP_SDW_CONFIG',
`}'
$1
)
dnl ACP_SDW_CONFIG_DATA(type, idx, rate, channel)
define(`ACP_SDW_CONFIG_DATA',
`SectionVendorTuples."'N_DAI_CONFIG($1$2)`_tuples" {'
`       tokens "sof_acp_sdw_tokens"'
`       tuples."word" {'
`               SOF_TKN_AMD_ACP_SDW_SAMPLERATE'		STR($3)
`		SOF_TKN_AMD_ACP_SDW_CH'		STR($4)
`       }'
`}'
`SectionData."'N_DAI_CONFIG($1$2)`_data" {'
`       tuples "'N_DAI_CONFIG($1$2)`_tuples"'
`}'
)

divert(0)dnl
