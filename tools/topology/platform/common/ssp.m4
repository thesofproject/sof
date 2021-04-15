divert(-1)

dnl SSP related macros

dnl SSP_CLOCK(clock, freq, codec_master, polarity)
dnl polarity is optional
define(`SSP_CLOCK',
	$1		STR($3)
	$1_freq	STR($2)
	`ifelse($4, `inverted', `$1_invert	"true"',`')')

dnl SSP_TDM(slots, width, tx_mask, rx_mask)
define(`SSP_TDM',
`tdm_slots	'STR($1)
`	tdm_slot_width	'STR($2)
`	tx_slots	'STR($3)
`	rx_slots	'STR($4)
)
dnl SSP_CONFIG(format, mclk, bclk, fsync, tdm, ssp_config_data)
define(`SSP_CONFIG',
`	format		"'$1`"'
`	'$2
`	'$3
`	'$4
`	'$5
`}'
$6
)

dnl SSP_QUIRK_LBM 64 = (1 << 6)
define(`SSP_QUIRK_LBM', 64)

dnl SSP_CONFIG_DATA(type, idx, valid bits, mclk_id, quirks, bclk_delay)
dnl mclk_id, quirks, bclk_delay are optional
define(`SSP_CONFIG_DATA',
`SectionVendorTuples."'N_DAI_CONFIG($1$2)`_tuples" {'
`	tokens "sof_ssp_tokens"'
`	tuples."word" {'
`		SOF_TKN_INTEL_SSP_SAMPLE_BITS'	STR($3)
`		SOF_TKN_INTEL_SSP_QUIRKS'	`ifelse(`$5', `', "0", STR($5))'
`		SOF_TKN_INTEL_SSP_BCLK_DELAY'	`ifelse(`$6', `', "0", STR($6))'
`	}'
`	tuples."short" {'
`		SOF_TKN_INTEL_SSP_MCLK_ID'	`ifelse(`$4', `', "0", STR($4))'
`	}'
`}'
`SectionData."'N_DAI_CONFIG($1$2)`_data" {'
`	tuples "'N_DAI_CONFIG($1$2)`_tuples"'
`}'
)

define(`MULTI_SSP_CONFIG',
`SectionHWConfig."'$1`" {'
`'
`	id		"'$2`"'
`'
`	format		"'$3`"'
`	'$4
`	'$5
`	'$6
`	'$7
`}'
$8
)

define(`SSP_MULTI_CONFIG_DATA',
`SectionVendorTuples."'$1`_tuples" {'
`	tokens "sof_ssp_tokens"'
`	tuples."word" {'
`		SOF_TKN_INTEL_SSP_SAMPLE_BITS'	STR($2)
`		SOF_TKN_INTEL_SSP_QUIRKS'	`ifelse(`$4', `', "0", STR($4))'
`		SOF_TKN_INTEL_SSP_BCLK_DELAY'	`ifelse(`$5', `', "0", STR($5))'
`		SOF_TKN_INTEL_SSP_CLKS_CONTROL' `ifelse(`$6', `', "0", STR($6))'
`	}'
`	tuples."short" {'
`		SOF_TKN_INTEL_SSP_MCLK_ID'	`ifelse(`$3', `', "0", STR($3))'
`		SOF_TKN_INTEL_SSP_FRAME_PULSE_WIDTH' `ifelse(`$7', `', "0", STR($7))'
`	}'
`	tuples."bool" {'
`		SOF_TKN_INTEL_SSP_TDM_PADDING_PER_SLOT' `ifelse(`$8', `', "false", STR($8))'
`	}'
`}'
`SectionData."'$1`" {'
`	tuples "'$1`_tuples"'
`}'
)

divert(0)dnl
