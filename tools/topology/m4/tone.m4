divert(-1)

dnl Define macro for siggen widget
DECLARE_SOF_RT_UUID("tone", tone_uuid, 0x04e3f894, 0x2c5c, 0x4f2e,
		 0x8d, 0xc1, 0x69, 0x4e, 0xea, 0xab, 0x53, 0xfa)

dnl N_TONE(name)
define(`N_TONE', `TONE'PIPELINE_ID`.'$1)

dnl W_TONE(name, format, periods_sink, periods_source, core, kcontrols_list)
define(`W_TONE',
`SectionVendorTuples."'N_TONE($1)`_tuples_uuid" {'
`	tokens "sof_comp_tokens"'
`	tuples."uuid" {'
`		SOF_TKN_COMP_UUID'		STR(tone_uuid)
`	}'
`}'
`SectionData."'N_TONE($1)`_data_uuid" {'
`	tuples "'N_TONE($1)`_tuples_uuid"'
`}'
`SectionVendorTuples."'N_TONE($1)`_tuples_w" {'
`	tokens "sof_comp_tokens"'
`	tuples."word" {'
`		SOF_TKN_COMP_PERIOD_SINK_COUNT'		STR($3)
`		SOF_TKN_COMP_PERIOD_SOURCE_COUNT'	STR($4)
`		SOF_TKN_COMP_CORE_ID'			STR($5)
`	}'
`}'
`SectionVendorTuples."'N_TONE($1)`_tone_tuples_w" {'
`	tokens "sof_tone_tokens"'
`	tuples."word" {'
`		SOF_TKN_TONE_SAMPLE_RATE'		ifdef(TONE_SAMPLE_RATE, STR(TONE_SAMPLE_RATE), "48000")
`	}'
`}'
`SectionData."'N_TONE($1)`_data_w" {'
`	tuples "'N_TONE($1)`_tuples_w"'
`	tuples "'N_TONE($1)`_tone_tuples_w"'
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
`		"'N_TONE($1)`_data_uuid"'
`		"'N_TONE($1)`_data_w"'
`		"'N_TONE($1)`_data_str"'
`	]'
`	mixer ['
		$6
`	]'
`}')

divert(0)dnl
