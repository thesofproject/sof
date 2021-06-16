divert(-1)

dnl Define macro for Mixer widget
DECLARE_SOF_RT_UUID("mixer", mixer_uuid, 0xbc06c037, 0x12aa, 0x417c,
		 0x9a, 0x97, 0x89, 0x28, 0x2e, 0x32, 0x1a, 0x76)

dnl N_MIXER(name)
define(`N_MIXER', `MIXER'PIPELINE_ID`.'$1)

dnl Pipe Buffer name in pipeline (pipeline, buffer)
define(`NPIPELINE_MIXER', `MIXER'$1`.'$2)

dnl W_MIXER(name, format, periods_sink, periods_source, core)
define(`W_MIXER',
`SectionVendorTuples."'N_MIXER($1)`_tuples_uuid" {'
`	tokens "sof_comp_tokens"'
`	tuples."uuid" {'
`		SOF_TKN_COMP_UUID'		STR(mixer_uuid)
`	}'
`}'
`SectionData."'N_MIXER($1)`_data_uuid" {'
`	tuples "'N_MIXER($1)`_tuples_uuid"'
`}'
`SectionVendorTuples."'N_MIXER($1)`_tuples_w" {'
`	tokens "sof_comp_tokens"'
`	tuples."word" {'
`		SOF_TKN_COMP_PERIOD_SINK_COUNT'		STR($3)
`		SOF_TKN_COMP_PERIOD_SOURCE_COUNT'	STR($4)
`		SOF_TKN_COMP_CORE_ID'			STR($5)
`	}'
`}'
`SectionData."'N_MIXER($1)`_data_w" {'
`	tuples "'N_MIXER($1)`_tuples_w"'
`}'
`SectionVendorTuples."'N_MIXER($1)`_tuples_str" {'
`	tokens "sof_comp_tokens"'
`	tuples."string" {'
`		SOF_TKN_COMP_FORMAT'	STR($2)
`	}'
`}'
`SectionData."'N_MIXER($1)`_data_str" {'
`	tuples "'N_MIXER($1)`_tuples_str"'
`}'
`SectionWidget."'N_MIXER($1)`" {'
`	index "'PIPELINE_ID`"'
`	type "mixer"'
`	no_pm "true"'
`	data ['
`		"'N_MIXER($1)`_data_uuid"'
`		"'N_MIXER($1)`_data_w"'
`		"'N_MIXER($1)`_data_str"'
`	]'
`}')

divert(0)dnl
