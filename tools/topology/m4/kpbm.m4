divert(-1)

dnl Define macro for Key Phrase Buffer Manager(kpbm) widget

dnl kpbm Name)
define(`N_KPBM', `KPBM'$2`.'$1)

dnl W_KPBM(name, format, periods_sink, periods_source, pipeline_id)
define(`W_KPBM',
`SectionVendorTuples."'N_KPBM($1, $5)`_tuples_w" {'
`	tokens "sof_comp_tokens"'
`	tuples."word" {'
`		SOF_TKN_COMP_PERIOD_SINK_COUNT'		STR($3)
`		SOF_TKN_COMP_PERIOD_SOURCE_COUNT'	STR($4)
`	}'
`}'
`SectionData."'N_KPBM($1, $5)`_data_w" {'
`	tuples "'N_KPBM($1, $5)`_tuples_w"'
`}'
`SectionVendorTuples."'N_KPBM($1, $5)`_tuples_str" {'
`	tokens "sof_comp_tokens"'
`	tuples."string" {'
`		SOF_TKN_COMP_FORMAT'	STR($2)
`	}'
`}'
`SectionVendorTuples."'N_KPBM($1)`_process_tuples_str" {'
`	tokens "sof_process_tokens"'
`	tuples."string" {'
`		SOF_TKN_PROCESS_TYPE'	"KPB"
`	}'
`}'
`SectionData."'N_KPBM($1, $5)`_data_str" {'
`	tuples "'N_KPBM($1, $5)`_tuples_str"'
`	tuples "'N_KPBM($1)`_process_tuples_str"'
`}'
`SectionWidget."'N_KPBM($1, $5)`" {'
`	index STR($5)'
`	type "effect"'
`	no_pm "true"'
`	data ['
`		"'N_KPBM($1, $5)`_data_w"'
`		"'N_KPBM($1, $5)`_data_str"'
`	]'
`}')

divert(0)dnl
