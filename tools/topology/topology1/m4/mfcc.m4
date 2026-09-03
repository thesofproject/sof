divert(-1)

dnl Define macro for MFCC widget
DECLARE_SOF_RT_UUID("mfcc", mfcc_uuid, 0xdb10a773, 0x1aa4, 0x4cea,
		    0xa2, 0x1f, 0x2d, 0x57, 0xa5, 0xc9, 0x82, 0xeb)

dnl N_MFCC(name)
define(`N_MFCC', `MFCC'PIPELINE_ID`.'$1)

dnl W_MFCC(name, format, periods_sink, periods_source, core, kcontrols_list)
define(`W_MFCC',
`SectionVendorTuples."'N_MFCC($1)`_tuples_uuid" {'
`	tokens "sof_comp_tokens"'
`	tuples."uuid" {'
`		SOF_TKN_COMP_UUID'		STR(mfcc_uuid)
`	}'
`}'
`SectionData."'N_MFCC($1)`_data_uuid" {'
`	tuples "'N_MFCC($1)`_tuples_uuid"'
`}'
`SectionVendorTuples."'N_MFCC($1)`_tuples_w" {'
`	tokens "sof_comp_tokens"'
`	tuples."word" {'
`		SOF_TKN_COMP_PERIOD_SINK_COUNT'		STR($3)
`		SOF_TKN_COMP_PERIOD_SOURCE_COUNT'	STR($4)
`		SOF_TKN_COMP_CORE_ID'			STR($5)
`	}'
`}'
`SectionData."'N_MFCC($1)`_data_w" {'
`	tuples "'N_MFCC($1)`_tuples_w"'
`}'
`SectionVendorTuples."'N_MFCC($1)`_tuples_str" {'
`	tokens "sof_comp_tokens"'
`	tuples."string" {'
`		SOF_TKN_COMP_FORMAT'	STR($2)
`	}'
`}'
`SectionData."'N_MFCC($1)`_data_str" {'
`	tuples "'N_MFCC($1)`_tuples_str"'
`}'
`SectionVendorTuples."'N_MFCC($1)`_tuples_str_type" {'
`	tokens "sof_process_tokens"'
`	tuples."string" {'
`		SOF_TKN_PROCESS_TYPE'	"MFCC"
`	}'
`}'
`SectionData."'N_MFCC($1)`_data_str_type" {'
`	tuples "'N_MFCC($1)`_tuples_str_type"'
`}'
`SectionWidget."'N_MFCC($1)`" {'
`	index "'PIPELINE_ID`"'
`	type "effect"'
`	no_pm "true"'
`	data ['
`		"'N_MFCC($1)`_data_uuid"'
`		"'N_MFCC($1)`_data_w"'
`		"'N_MFCC($1)`_data_str"'
`		"'N_MFCC($1)`_data_str_type"'
`	]'
`	bytes ['
		$6
`	]'
`}')

divert(0)dnl
