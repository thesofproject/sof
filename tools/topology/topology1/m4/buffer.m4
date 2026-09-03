divert(-1)

dnl Define the macro for buffer widget

dnl N_BUFFER(name)
define(`N_BUFFER', `BUF'PIPELINE_ID`.'$1)

dnl W_BUFFER(name, size, capabilities, [core], [flags])
define(`W_BUFFER',
`SectionVendorTuples."'N_BUFFER($1)`_tuples" {'
`	tokens "sof_buffer_tokens"'
`	tuples."word" {'
`		SOF_TKN_BUF_SIZE'	STR($2)
`		SOF_TKN_BUF_CAPS'	STR($3)
`ifelse(`$#', `5',
`		SOF_TKN_BUF_FLAGS'	STR($5)
,` ')'
`	}'
`}'
`SectionData."'N_BUFFER($1)`_data" {'
`	tuples "'N_BUFFER($1)`_tuples"'
`}'
`ifelse(`$#', `4',
`SectionVendorTuples."'N_BUFFER($1)`_comp_tuples" {'
`	tokens "sof_comp_tokens"'
`	tuples."word" {'
`		SOF_TKN_COMP_CORE_ID'	0
`	}'
`}'
`SectionData."'N_BUFFER($1)`_comp" {'
`	tuples "'N_BUFFER($1)`_comp_tuples"'
`}'
,` ')'
`SectionWidget."'N_BUFFER($1)`" {'
`	index "'PIPELINE_ID`"'
`	type "buffer"'
`	no_pm "true"'
`	data ['
`		"'N_BUFFER($1)`_data"'
`ifelse(`$#', `4',
`		"'N_BUFFER($1)`_comp"'
,` ')'
`	]'
`}')

dnl COMP_BUFFER_SIZE( num_periods, sample_size, channels, fmames)
define(`COMP_BUFFER_SIZE', `eval(`$1 * $2 * $3 * $4')')

dnl COMP_PERIOD_FRAMES( sample_rate, period_us)
dnl note: m4 eval arithmetic is 32bit signed, so split the 10^6
dnl       division to avoid overflow.
define(`COMP_PERIOD_FRAMES', `eval(`$1 / 100 * $2 / 10000')')

divert(0)dnl
