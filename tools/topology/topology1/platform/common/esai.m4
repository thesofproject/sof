divert(-1)

dnl ESAI related macros

dnl ESAI_CLOCK(clock, freq, codec_master, polarity)
dnl polarity is optional
define(`ESAI_CLOCK',
	$1		STR($3)
	$1_freq		STR($2))
	`ifelse($4, `inverted', `$1_invert	"true"',`')')

dnl ESAI_TDM(slots, width, tx_mask, rx_mask)
define(`ESAI_TDM',
`	tdm_slots	'STR($1)
`	tdm_slot_width	'STR($2)
`	tx_slots	'STR($3)
`	rx_slots	'STR($4)
)

dnl ESAI_CONFIG(format, mclk, bclk, fsync, tdm, esai_config_data)
define(`ESAI_CONFIG',
`	format		"'$1`"'
`	'$2
`	'$3
`	'$4
`	'$5
`}'
$6
)

dnl ESAI_CONFIG_DATA(type, idx, mclk_id)
dnl mclk_id is optional
define(`ESAI_CONFIG_DATA',
`SectionVendorTuples."'N_DAI_CONFIG($1$2)`_tuples" {'
`	tokens "sof_esai_tokens"'
`	tuples."short" {'
`		SOF_TKN_IMX_ESAI_MCLK_ID'	ifelse($3, `', "0", STR($3))
`	}'
`}'
`SectionData."'N_DAI_CONFIG($1$2)`_data" {'
`	tuples "'N_DAI_CONFIG($1$2)`_tuples"'
`}'
)

divert(0)dnl
