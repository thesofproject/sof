divert(-1)

dnl Virtual DAI related macros

dnl DAI_VIRTUAL_CLOCK(clock, freq, codec_provider, polarity)
dnl polarity is optional
define(`DAI_VIRTUAL_CLOCK',
	$1		STR($3)
	$1_freq		STR($2))
	`ifelse($4, `inverted', `$1_invert	"true"',`')')

dnl DAI_VIRTUAL_TDM(slots, width, tx_mask, rx_mask)
define(`DAI_VIRTUAL_TDM',
`	tdm_slots	'STR($1)
`	tdm_slot_width	'STR($2)
`	tx_slots	'STR($3)
`	rx_slots	'STR($4)
)

dnl DAI_VIRTUAL_CONFIG(format, mclk, bclk, fsync, tdm, config_data)
define(`DAI_VIRTUAL_CONFIG',
`	format		"'$1`"'
`	'$2
`	'$3
`	'$4
`	'$5
`}'
$6
)

dnl DAI_VIRTUAL_CONFIG_DATA(type, idx, dummy_id)
dnl dummy_id is optional
define(`DAI_VIRTUAL_CONFIG_DATA',
`SectionVendorTuples."'N_DAI_CONFIG($1$2)`_tuples" {'
`	tokens "sof_virtual_tokens"'
`	tuples."short" {'
`		SOF_TKN_DAI_VIRTUAL_MCLK_ID'	ifelse($3, `', "0", STR($3))
`	}'
`}'
`SectionData."'N_DAI_CONFIG($1$2)`_data" {'
`	tuples "'N_DAI_CONFIG($1$2)`_tuples"'
`}'
)

divert(0)dnl
