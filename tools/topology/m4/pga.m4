divert(-1)

dnl Define macro for PGA widget

dnl PGA name)
define(`N_PGA', `PGA'PIPELINE_ID`.'$1)

dnl W_PGA(name, format, periods_sink, periods_source, kcontrol0. kcontrol1...etc)
define(`W_PGA',
`SectionVendorTuples."'N_PGA($1)`_tuples_w" {'
`	tokens "sof_comp_tokens"'
`	tuples."word" {'
`		SOF_TKN_COMP_PERIOD_SINK_COUNT'		STR($3)
`		SOF_TKN_COMP_PERIOD_SOURCE_COUNT'	STR($4)
`	}'
`}'
`SectionData."'N_PGA($1)`_data_w" {'
`	tuples "'N_PGA($1)`_tuples_w"'
`}'
`SectionVendorTuples."'N_PGA($1)`_tuples_str" {'
`	tokens "sof_comp_tokens"'
`	tuples."string" {'
`		SOF_TKN_COMP_FORMAT'	STR($2)
`	}'
`}'
`SectionData."'N_PGA($1)`_data_str" {'
`	tuples "'N_PGA($1)`_tuples_str"'
`}'
`SectionWidget."'N_PGA($1)`" {'
`	index "'PIPELINE_ID`"'
`	type "pga"'
`	no_pm "true"'
`	data ['
`		"'N_PGA($1)`_data_w"'
`		"'N_PGA($1)`_data_str"'
`	]'
`	mixer ['
		$5
`	]'

`}')

divert(0)dnl
