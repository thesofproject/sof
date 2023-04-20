divert(-1)

dnl ACP_SDW related macros

dnl ACP_CLOCK(clock, freq, codec_master, polarity)
dnl polarity is optional
define(`ACP_CLOCK',
        $1              STR($3)
        $1_freq         STR($2))
        `ifelse($4, `inverted', `$1_invert      "true"',`')')

dnl ACP_TDM(slots, width, tx_mask, rx_mask)
define(`ACP_TDM',
`       tdm_slots       'STR($1)
`       tdm_slot_width  'STR($2)
`       tx_slots        'STR($3)
`       rx_slots        'STR($4)
)

dnl ACP_CONFIG(format, mclk, bclk, fsync, tdm, sdw_config_data)
define(`ACP_SDW_CONFIG',
`       format          "'$1`"'
`       '$2
`       '$3
`       '$4
`       '$5
`}'
$6
)

dnl ACP_SDW_VIRTUAL_CONFIG(format, mclk, bclk, fsync, tdm, sdw_config_data)
define(`ACP_SDW_VIRTUAL_CONFIG',
`       format          "'$1`"'
`       '$2
`       '$3
`       '$4
`       '$5
`}'
$6
)
dnl ACP_SDW_CONFIG_DATA(type, idx, rate, channel, i2s_tdm_mode)
#i2s_tdm_mode 1-> tdm mode, 0->i2s mode
define(`ACP_SDW_CONFIG_DATA',
`SectionVendorTuples."'N_DAI_CONFIG($1$2)`_tuples" {'
`       tokens "sof_acp_sdw_tokens"'
`       tuples."word" {'
`               SOF_TKN_AMD_ACP_SDW_RATE'		STR($3)
`		SOF_TKN_AMD_ACP_SDW_CH'		STR($4)
`       }'
`}'
`SectionData."'N_DAI_CONFIG($1$2)`_data" {'
`       tuples "'N_DAI_CONFIG($1$2)`_tuples"'
`}'
)

dnl ACP_SDW_VIRTUAL_CONFIG_DATA(type, idx, rate, channel, i2s_tdm_mode)
#i2s_tdm_mode 1-> tdm mode, 0->i2s mode
define(`ACP_SDW_VIRTUAL_CONFIG_DATA',
`SectionVendorTuples."'N_DAI_CONFIG($1$2)`_tuples" {'
`       tokens "sof_acp_sdw_tokens"'
`       tuples."word" {'
`               SOF_TKN_AMD_ACP_SDW_RATE'		STR($3)
`		SOF_TKN_AMD_ACP_SDW_CH'		STR($4)
`       }'
`}'
`SectionData."'N_DAI_CONFIG($1$2)`_data" {'
`       tuples "'N_DAI_CONFIG($1$2)`_tuples"'
`}'
)

divert(0)dnl
