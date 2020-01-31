divert(-1)

dnl Define macro for demux widget

dnl Mux name)
define(`N_MUXDEMUX', `MUXDEMUX'PIPELINE_ID`.'$1)

dnl CH_MAP_COEFF(coeff)
define(`CH_MAP_COEFF',
`	tuples."word.$1" {'
`	SOF_TKN_CHANNEL_MAP_COEFF'		STR($2)
`	}')

dnl CH_MAP(name, id, ext_id, mask)
define(`CH_MAP',
`	tuples."word.$1" {'
`		SOF_TKN_CHANNEL_MAP_INDEX'		STR($2)
`		SOF_TKN_CHANNEL_MAP_EXT_ID'		STR($3)
`		SOF_TKN_CHANNEL_MAP_MASK'		STR($4)
`	}'
	$5
)

dnl W_MUXDEMUX(name, format, periods_sink, periods_source, num_chmaps, chmap_list)
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
`		SOF_TKN_COMP_FORMAT'	STR($2)
`	}'
`}'
`SectionData."'N_MUXDEMUX($1)`_data_str" {'
`	tuples "'N_MUXDEMUX($1)`_tuples_str"'
`}'
`SectionVendorTuples."'N_MUXDEMUX($1)`_mux_tuples_type" {'
`      tokens "sof_process_tokens"'
`      tuples."string" {'
`		SOF_TKN_PROCESS_TYPE   "MUX"'
`      }'
`}'
`SectionData."'N_MUXDEMUX($1)`_data_type" {'
`	tuples "'N_MUXDEMUX($1)`_mux_tuples_type"'
`}'
`SectionVendorTuples."'N_MUXDEMUX($1)`_mux_data_tuples_w" {'
`	tokens "sof_chmap_tokens"'
`	tuples."word" {'
`		SOF_TKN_STREAM_MAP_NUM_CH_MAPS' 		STR($5)
`	}'
	$6
`}'
`SectionData."'N_MUXDEMUX($1)`_mux_data_w" {'
`	tuples "'N_MUXDEMUX($1)`_mux_data_tuples_w"'
`}'
`SectionWidget."'N_MUXDEMUX($1)`" {'
`	index "'PIPELINE_ID`"'
`	type "effect"'
`	no_pm "true"'
`	data ['
`		"'N_MUXDEMUX($1)`_data_w"'
`		"'N_MUXDEMUX($1)`_data_str"'
`		"'N_MUXDEMUX($1)`_data_type"'
`		"'N_MUXDEMUX($1)`_mux_data_w"'
`	]'
`}')

divert(0)dnl
