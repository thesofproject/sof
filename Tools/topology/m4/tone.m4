divert(-1)

dnl Define macro for siggen widget

dnl Tone name)
define(`N_TONE', `TONE'PIPELINE_ID`.'$1)

dnl W_TONE(name, format, periods_sink, periods_source, preload, kcontrols_list)
define(`W_TONE',
`SectionVendorTuples."'N_TONE($1)`_tuples_w" {'
`	tokens "sof_comp_tokens"'
`	tuples."word" {'
`		SOF_TKN_COMP_PERIOD_SINK_COUNT'		STR($3)
`		SOF_TKN_COMP_PERIOD_SOURCE_COUNT'	STR($4)
`		SOF_TKN_COMP_PRELOAD_COUNT'		STR($5)
`	}'
`}'
`SectionData."'N_TONE($1)`_data_w" {'
`	tuples "'N_TONE($1)`_tuples_w"'
`}'
`SectionVendorTuples."'N_TONE($1)`_tuples_str" {'
`	tokens "sof_comp_tokens"'
`	tuples."string" {'
`		SOF_TKN_COMP_FORMAT'	STR($2)
`	}'
`}'
`SectionData."'N_TONE($1)`_data_str" {'
`	tuples "'N_TONE($1)`_tuples_str"'
`}'
`SectionWidget."'N_TONE($1)`" {'
`	index "'PIPELINE_ID`"'
`	type "siggen"'
`	no_pm "true"'
`	data ['
`		"'N_TONE($1)`_data_w"'
`		"'N_TONE($1)`_data_str"'
`	]'
`	mixer ['
		$6
`	]'
`}')

divert(0)dnl
