divert(-1)

dnl SAI related macros

dnl SAI_CLOCK(clock, freq, codec_master)
define(`SAI_CLOCK',
	$1		STR($3)
	$1_freq	STR($2))


dnl SAI_TDM(slots, width, tx_mask, rx_mask)
define(`SAI_TDM',
`tdm_slots	'STR($1)
`	tdm_slot_width	'STR($2)
`	tx_slots	'STR($3)
`	rx_slots	'STR($4)
)
dnl SAI_CONFIG(format, mclk, bclk, fsync, tdm, sai_config_data)
define(`SAI_CONFIG',
`	format		"'$1`"'
`	'$2
`	'$3
`	'$4
`	'$5
`}'
$6
)

dnl SAI_CONFIG_DATA(type, idx, valid bits, mclk_id)
dnl mclk_id is optional
define(`SAI_CONFIG_DATA',
`SectionVendorTuples."'N_DAI_CONFIG($1$2)`_tuples" {'
`	tokens "sof_sai_tokens"'
`	tuples."word" {'
`		SOF_TKN_INTEL_SAI_SAMPLE_BITS'	STR($3)
`	}'
`	tuples."short" {'
`		SOF_TKN_INTEL_SAI_MCLK_ID'	ifelse($4, `', "0", STR($4))
`	}'
`}'
`SectionData."'N_DAI_CONFIG($1$2)`_data" {'
`	tuples "'N_DAI_CONFIG($1$2)`_tuples"'
`}'
)

divert(0)dnl
