divert(-1)

dnl Define macro for demux widget

dnl Mux name)
define(`N_MUXDEMUX', `MUXDEMUX'PIPELINE_ID`.'$1)

dnl W_MUXDEMUX(name, mux/demux, format, periods_sink, periods_source, kcontrol_list)
define(`W_MUXDEMUX',
`SectionVendorTuples."'N_MUXDEMUX($1)`_tuples_w" {'
`	tokens "sof_comp_tokens"'
`	tuples."word" {'
`		SOF_TKN_COMP_PERIOD_SINK_COUNT'		STR($4)
`		SOF_TKN_COMP_PERIOD_SOURCE_COUNT'	STR($5)
`	}'
`}'
`SectionData."'N_MUXDEMUX($1)`_data_w" {'
`	tuples "'N_MUXDEMUX($1)`_tuples_w"'
`}'
`SectionVendorTuples."'N_MUXDEMUX($1)`_tuples_str" {'
`	tokens "sof_comp_tokens"'
`	tuples."string" {'
`		SOF_TKN_COMP_FORMAT'	STR($3)
`	}'
`}'
`SectionVendorTuples."'N_MUXDEMUX($1)`_mux_process_tuples_str" {'
`	tokens "sof_process_tokens"'
`	tuples."string" {'
`ifelse(`$2', `0', `		SOF_TKN_PROCESS_TYPE'	"MUX", `		SOF_TKN_PROCESS_TYPE'	"DEMUX")'
`	}'
`}'
`SectionData."'N_MUXDEMUX($1)`_data_str" {'
`	tuples "'N_MUXDEMUX($1)`_tuples_str"'
`	tuples "'N_MUXDEMUX($1)`_mux_process_tuples_str"'
`}'
`SectionWidget."'N_MUXDEMUX($1)`" {'
`	index "'PIPELINE_ID`"'
`	type "effect"'
`	no_pm "true"'
`	data ['
`		"'N_MUXDEMUX($1)`_data_w"'
`		"'N_MUXDEMUX($1)`_data_str"'
`	]'
`	bytes ['
		$6
`	]'
`}')

divert(0)dnl
