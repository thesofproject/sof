divert(-1)

dnl Defines the macro for ASRC widget
DECLARE_SOF_RT_UUID("asrc", asrc_uuid, 0xc8ec72f6, 0x8526, 0x4faf,
		 0x9d, 0x39, 0xa2, 0x3d, 0x0b, 0x54, 0x1d, 0xe2)

dnl ASRC name)
define(`N_ASRC', `ASRC'PIPELINE_ID`.'$1)

dnl W_ASRC(name, format, periods_sink, periods_source, data)
define(`W_ASRC',
`SectionVendorTuples."'N_ASRC($1)`_tuples_uuid" {'
`	tokens "sof_comp_tokens"'
`	tuples."uuid" {'
`		SOF_TKN_COMP_UUID'		STR(asrc_uuid)
`	}'
`}'
`SectionData."'N_ASRC($1)`_data_uuid" {'
`	tuples "'N_ASRC($1)`_tuples_uuid"'
`}'
`SectionVendorTuples."'N_ASRC($1)`_tuples_w" {'
`	tokens "sof_comp_tokens"'
`	tuples."word" {'
`		SOF_TKN_COMP_PERIOD_SINK_COUNT'		STR($3)
`		SOF_TKN_COMP_PERIOD_SOURCE_COUNT'	STR($4)
`	}'
`}'
`SectionData."'N_ASRC($1)`_data_w" {'
`	tuples "'N_ASRC($1)`_tuples_w"'
`}'
`SectionVendorTuples."'N_ASRC($1)`_tuples_str" {'
`	tokens "sof_comp_tokens"'
`	tuples."string" {'
`		SOF_TKN_COMP_FORMAT'	STR($2)
`	}'
`}'
`SectionData."'N_ASRC($1)`_data_str" {'
`	tuples "'N_ASRC($1)`_tuples_str"'
`}'
`SectionWidget."'N_ASRC($1)`" {'
`	index "'PIPELINE_ID`"'
`	type "asrc"'
`	no_pm "true"'
`	data ['
`		"'N_ASRC($1)`_data_uuid"'
`		"'N_ASRC($1)`_data_w"'
`		"'N_ASRC($1)`_data_str"'
`		"'$5`"'
`	]'
`}')

divert(0)dnl
