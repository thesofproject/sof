divert(-1)

dnl Define macro for PGA widget

dnl N_PGA(name)
define(`N_PGA', `PGA'PIPELINE_ID`.'$1)

dnl W_PGA(name, format, periods_sink, periods_source, kcontrol0. kcontrol1...etc)
define(`W_PGA',
`SectionVendorTuples."'$1`_tuples_w" {'
`	tokens "sof_comp_tokens"'
`	tuples."word" {'
`		SOF_TKN_COMP_PERIOD_SINK_COUNT'		STR($3)
`		SOF_TKN_COMP_PERIOD_SOURCE_COUNT'	STR($4)
`	}'
`}'
`SectionData."'$1`_data_w" {'
`	tuples "'$1`_tuples_w"'
`}'
`SectionVendorTuples."'$1`_tuples_str" {'
`	tokens "sof_comp_tokens"'
`	tuples."string" {'
`		SOF_TKN_COMP_FORMAT'	STR($2)
`	}'
`}'
`SectionData."'$1`_data_str" {'
`	tuples "'$1`_tuples_str"'
`}'
`SectionWidget."'$1`" {'
`	index "'PIPELINE_ID`"'
`	type "pga"'
`	no_pm "true"'
`	data ['
`		"'$1`_data_w"'
`		"'$1`_data_str"'
`		"'$5`"'
`	]'
`	mixer ['
		$6
`	]'

`}')

divert(0)dnl
