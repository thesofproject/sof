divert(-1)

dnl ACPHS related macros

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

dnl ACP_CONFIG(format, mclk, bclk, fsync, tdm, hs_config_data)
define(`ACPHS_CONFIG',
`       format          "'$1`"'
`       '$2
`       '$3
`       '$4
`       '$5
`}'
$6
)

dnl ACPHS_CONFIG_DATA(type, idx, rate, channel)
define(`ACPHS_CONFIG_DATA',
`SectionVendorTuples."'N_DAI_CONFIG($1$2)`_tuples" {'
`       tokens "sof_acphs_tokens"'
`       tuples."word" {'
`               SOF_TKN_AMD_ACPHS_RATE'		STR($3)
`		SOF_TKN_AMD_ACPHS_CH'		STR($4)
`       }'
`}'
`SectionData."'N_DAI_CONFIG($1$2)`_data" {'
`       tuples "'N_DAI_CONFIG($1$2)`_tuples"'
`}'
)

divert(0)dnl
