divert(-1)

dnl Define macro for PGA widget
DECLARE_SOF_RT_UUID("volume", volume_uuid, 0xb77e677e, 0x5ff4, 0x4188,
		 0xaf, 0x14, 0xfb, 0xa8, 0xbd, 0xbf, 0x86, 0x82)

dnl N_PGA(name)
define(`N_PGA', `PGA'PIPELINE_ID`.'$1)

dnl W_PGA(name, format, periods_sink, periods_source, core, kcontrol0. kcontrol1...etc)
define(`W_PGA',
`SectionVendorTuples."'N_PGA($1)`_tuples_uuid" {'
`	tokens "sof_comp_tokens"'
`	tuples."uuid" {'
`		SOF_TKN_COMP_UUID'		STR(volume_uuid)
`	}'
`}'
`SectionData."'N_PGA($1)`_data_uuid" {'
`	tuples "'N_PGA($1)`_tuples_uuid"'
`}'
`SectionVendorTuples."'N_PGA($1)`_tuples_w" {'
`	tokens "sof_comp_tokens"'
`	tuples."word" {'
`		SOF_TKN_COMP_PERIOD_SINK_COUNT'		STR($3)
`		SOF_TKN_COMP_PERIOD_SOURCE_COUNT'	STR($4)
`		SOF_TKN_COMP_CORE_ID'			STR($6)
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
`SectionWidget.ifdef(`PGA_NAME', "`PGA_NAME'", "`N_PGA($1)'") {'
`	index "'PIPELINE_ID`"'
`	type "pga"'
`	no_pm "true"'
`	data ['
`		"'N_PGA($1)`_data_uuid"'
`		"'N_PGA($1)`_data_w"'
`		"'N_PGA($1)`_data_str"'
`		"'$5`"'
`	]'
`	mixer ['
		$7
`	]'

`}')

divert(0)dnl
