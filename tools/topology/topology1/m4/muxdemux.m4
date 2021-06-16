divert(-1)

dnl Define macro for mux/demux widget
DECLARE_SOF_RT_UUID("mux", mux_uuid, 0xc607ff4d, 0x9cb6, 0x49dc,
		 0xb6, 0x78, 0x7d, 0xa3, 0xc6, 0x3e, 0xa5, 0x57)
DECLARE_SOF_RT_UUID("demux", demux_uuid, 0xc4b26868, 0x1430, 0x470e,
		 0xa0, 0x89, 0x15, 0xd1, 0xc7, 0x7f, 0x85, 0x1a)

dnl Hard coded values for mux/demux config blob
define(mux_sof_magic, 0x00464F53)
define(mux_stream_struct_size, 16)
define(mux_config_struct_size, 8)

dnl Fill bytes of struct mux_stream_config (mux.h)
dnl reserved fields in the struct are set to 0
define(`ROUTE_MATRIX',
	`PRINT_BYTES_4($1),PRINT_BYTE(0),PRINT_BYTE($2),PRINT_BYTE($3),'dnl
`PRINT_BYTE($4),'
	`PRINT_BYTE($5),PRINT_BYTE($6),PRINT_BYTE($7),PRINT_BYTE($8),'dnl
`PRINT_BYTE($9),PRINT_BYTE(0),PRINT_BYTE(0),PRINT_BYTE(0)')

dnl Fill bytes of mux/demux config binary blob
dnl blob is made of sof_abi_hdr (header.h), sof_mux_config (mux.h) and
dnl list of ROUTE_MATRIXes.
dnl reserved fields in the struct are set to 0
define(`MUXDEMUX_CONFIG',
`SectionData.STR($1) {'
`bytes "'`PRINT_BYTES_4(mux_sof_magic),PRINT_BYTES_4(0),'
	`PRINT_BYTES_4(eval(mux_config_struct_size + (mux_stream_struct_size * $2))),'dnl
`PRINT_BYTES_4(SOF_ABI_VERSION),'
	`PRINT_BYTES_4(0),PRINT_BYTES_4(0),'
	`PRINT_BYTES_4(0),PRINT_BYTES_4(0),'
	`PRINT_BYTES_2(0),PRINT_BYTES_2(0),PRINT_BYTES_2($2),PRINT_BYTES_2(0),'
	`$3'`"'
	`}'
)

dnl N_MUXDEMUX(name)
define(`N_MUXDEMUX', `MUXDEMUX'PIPELINE_ID`.'$1)

dnl W_MUXDEMUX(name, mux/demux, format, periods_sink, periods_source, core, kcontrol_list)
define(`W_MUXDEMUX',
`SectionVendorTuples."'N_MUXDEMUX($1)`_tuples_uuid" {'
`	tokens "sof_comp_tokens"'
`	tuples."uuid" {'
`ifelse(`$2', `0',`		SOF_TKN_COMP_UUID'	STR(mux_uuid),`		SOF_TKN_COMP_UUID'	STR(demux_uuid))'
`	}'
`}'
`SectionData."'N_MUXDEMUX($1)`_data_uuid" {'
`	tuples "'N_MUXDEMUX($1)`_tuples_uuid"'
`}'
`SectionVendorTuples."'N_MUXDEMUX($1)`_tuples_w" {'
`	tokens "sof_comp_tokens"'
`	tuples."word" {'
`		SOF_TKN_COMP_PERIOD_SINK_COUNT'		STR($4)
`		SOF_TKN_COMP_PERIOD_SOURCE_COUNT'	STR($5)
`		SOF_TKN_COMP_CORE_ID'			STR($6)
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
`		"'N_MUXDEMUX($1)`_data_uuid"'
`		"'N_MUXDEMUX($1)`_data_w"'
`		"'N_MUXDEMUX($1)`_data_str"'
`	]'
`	bytes ['
		$7
`	]'
`}')

divert(0)dnl
