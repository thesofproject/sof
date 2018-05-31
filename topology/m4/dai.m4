divert(-1)

dnl Define macros for DAI IN/OUT widgets and DAI config

dnl DAI name)
define(`N_DAI', DAI_NAME)
define(`N_DAI_OUT', DAI_NAME`.OUT')
define(`N_DAI_IN', DAI_NAME`.IN')

dnl W_DAI_OUT(type, index, dai_link, format, periods_sink, periods_source, preload, data)
define(`W_DAI_OUT',
`SectionVendorTuples."'N_DAI_OUT($2)`_tuples_w_comp" {'
`	tokens "sof_comp_tokens"'
`	tuples."word" {'
`		SOF_TKN_COMP_PERIOD_SINK_COUNT'		STR($5)
`		SOF_TKN_COMP_PERIOD_SOURCE_COUNT'	STR($6)
`		SOF_TKN_COMP_PRELOAD_COUNT'		STR($7)
`	}'
`}'
`SectionData."'N_DAI_OUT($2)`_data_w_comp" {'
`	tuples "'N_DAI_OUT($2)`_tuples_w_comp"'
`}'
`SectionVendorTuples."'N_DAI_OUT($2)`_tuples_w" {'
`	tokens "sof_dai_tokens"'
`	tuples."word" {'
`		SOF_TKN_DAI_INDEX'	$2
`	}'
`}'
`SectionData."'N_DAI_OUT($2)`_data_w" {'
`	tuples "'N_DAI_OUT($2)`_tuples_w"'
`}'
`SectionVendorTuples."'N_DAI_OUT($2)`_tuples_str" {'
`	tokens "sof_dai_tokens"'
`	tuples."string" {'
`		SOF_TKN_DAI_TYPE'	$1
`	}'
`}'
`SectionData."'N_DAI_OUT($2)`_data_str" {'
`	tuples "'N_DAI_OUT($2)`_tuples_str"'
`}'
`SectionVendorTuples."'N_DAI_OUT($2)`_tuples_comp_str" {'
`	tokens "sof_comp_tokens"'
`	tuples."string" {'
`		SOF_TKN_COMP_FORMAT'	STR($4)
`	}'
`}'
`SectionData."'N_DAI_OUT($2)`_data_comp_str" {'
`	tuples "'N_DAI_OUT($2)`_tuples_comp_str"'
`}'
`SectionWidget."'N_DAI_OUT`" {'
`	index "'PIPELINE_ID`"'
`	type "dai_in"'
`	stream_name' STR($3)
`	no_pm "true"'
`	data ['
`		"'N_DAI_OUT($2)`_data_w"'
`		"'N_DAI_OUT($2)`_data_w_comp"'
`		"'N_DAI_OUT($2)`_data_str"'
`		"'N_DAI_OUT($2)`_data_comp_str"'
`		"'$8`"'
`	]'
`}')

dnl W_DAI_IN(type, index, dai_link, format, periods_sink, periods_source, preload, data)
define(`W_DAI_IN',
`SectionVendorTuples."'N_DAI_IN($2)`_tuples_w_comp" {'
`	tokens "sof_comp_tokens"'
`	tuples."word" {'
`		SOF_TKN_COMP_PERIOD_SINK_COUNT'		STR($5)
`		SOF_TKN_COMP_PERIOD_SOURCE_COUNT'	STR($6)
`		SOF_TKN_COMP_PRELOAD_COUNT'		STR($7)
`	}'
`}'
`SectionData."'N_DAI_IN($2)`_data_w_comp" {'
`	tuples "'N_DAI_IN($2)`_tuples_w_comp"'
`}'
`SectionVendorTuples."'N_DAI_IN($2)`_tuples_w" {'
`	tokens "sof_dai_tokens"'
`	tuples."word" {'
`		SOF_TKN_DAI_INDEX'	$2
`	}'
`}'
`SectionData."'N_DAI_IN($2)`_data_w" {'
`	tuples "'N_DAI_IN($2)`_tuples_w"'
`}'
`SectionVendorTuples."'N_DAI_IN($2)`_tuples_str" {'
`	tokens "sof_dai_tokens"'
`	tuples."string" {'
`		SOF_TKN_DAI_TYPE'	$1
`	}'
`}'
`SectionData."'N_DAI_IN($2)`_data_str" {'
`	tuples "'N_DAI_IN($2)`_tuples_str"'
`}'
`SectionVendorTuples."'N_DAI_IN($2)`_tuples_comp_str" {'
`	tokens "sof_comp_tokens"'
`	tuples."string" {'
`		SOF_TKN_COMP_FORMAT'	STR($4)
`	}'
`}'
`SectionData."'N_DAI_IN($2)`_data_comp_str" {'
`	tuples "'N_DAI_IN($2)`_tuples_comp_str"'
`}'
`SectionWidget."'N_DAI_IN`" {'
`	index "'PIPELINE_ID`"'
`	type "dai_out"'
`	stream_name' STR($3)
`	no_pm "true"'
`	data ['
`		"'N_DAI_IN($2)`_data_w"'
`		"'N_DAI_IN($2)`_data_w_comp"'
`		"'N_DAI_IN($2)`_data_str"'
`		"'N_DAI_IN($2)`_data_comp_str"'
`		"'$8`"'
`	]'
`}')

dnl D_DAI(id, playback, capture, data))
define(`D_DAI', `SectionDAI."'N_DAI`" {'
`	index "'PIPELINE_ID`"'
`	id "'$1`"'
`	playback "'$2`"'
`	capture "'$3`"'
`}')

dnl DAI_CLOCK(clock, freq, codec_master)
define(`DAI_CLOCK',
	$1		STR($3)
	$1_freq	STR($2))


dnl DAI_TDM(slots, width, tx_mask, rx_mask)
define(`DAI_TDM',
`tdm_slots	'STR($1)
`	tdm_slot_width	'STR($2)
`	tx_slots	'STR($3)
`	rx_slots	'STR($4)
)
dnl SSP_CONFIG(format, mclk, bclk, fsync, tdm, ssp sample bits)
define(`SSP_CONFIG',
`	format		"'$1`"'
`	'$2
`	'$3
`	'$4
`	'$5
`}'
$6
)

dnl SSP_SAMPLE_BITS(type, idx, valid bits)
define(`SSP_SAMPLE_BITS',
`SectionVendorTuples."'N_DAI_CONFIG($1$2)`_tuples" {'
`	tokens "sof_dai_tokens"'
`	tuples."word" {'
`		SOF_TKN_DAI_SAMPLE_BITS'	STR($3)
`	}'
`}'
`SectionData."'N_DAI_CONFIG($1$2)`_data" {'
`	tuples "'N_DAI_CONFIG($1$2)`_tuples"'
`}'
)

dnl PDM_TUPLES(pdm ctrl id, mic_a_enable, mic_b_enable, polarity_a, polarity_b,
dnl	       clk_egde, skew)
define(`PDM_TUPLES',
`	tuples."short.pdm$1" {'
`		SOF_TKN_INTEL_DMIC_PDM_CTRL_ID'		STR($1)
`		SOF_TKN_INTEL_DMIC_PDM_MIC_A_Enable'	STR($2)
`		SOF_TKN_INTEL_DMIC_PDM_MIC_B_Enable'	STR($3)
`		SOF_TKN_INTEL_DMIC_PDM_POLARITY_A'	STR($4)
`		SOF_TKN_INTEL_DMIC_PDM_POLARITY_B'	STR($5)
`		SOF_TKN_INTEL_DMIC_PDM_CLK_EDGE'	STR($6)
`		SOF_TKN_INTEL_DMIC_PDM_SKEW'		STR($7)
`	}'
)

dnl PDM_CONFIG(type, idx, pdm tuples list)
define(`PDM_CONFIG',
`SectionVendorTuples."'N_DAI_CONFIG($1$2)`_pdm_tuples" {'
`	tokens "sof_dmic_pdm_tokens"'
$3
`}'
)

dnl DMIC currently only supports 16 bit or 32-bit word length
dnl DMIC_WORD_LENGTH(frame format)
define(`DMIC_WORD_LENGTH',
`ifelse($1, `s16le', 16, $1, `s32le', 32, `')')

dnl DMIC_CONFIG(driver_version, clk_min, clk_mac, duty_min, duty_max,
dnl		req pdm count, sample_rate,
dnl		fifo word length, type, idx, pdm controller config)
define(`DMIC_CONFIG',
`SectionVendorTuples."'N_DAI_CONFIG($9$10)`_dmic_tuples" {'
`	tokens "sof_dmic_tokens"'
`	tuples."word" {'
`		SOF_TKN_INTEL_DMIC_DRIVER_VERSION'	STR($1)
`		SOF_TKN_INTEL_DMIC_CLK_MIN'		STR($2)
`		SOF_TKN_INTEL_DMIC_CLK_MAX'		STR($3)
`		SOF_TKN_INTEL_DMIC_DUTY_MIN'		STR($4)
`		SOF_TKN_INTEL_DMIC_DUTY_MAX'		STR($5)
`		SOF_TKN_INTEL_DMIC_NUM_PDM_ACTIVE'	STR($6)
`		SOF_TKN_INTEL_DMIC_SAMPLE_RATE'		STR($7)
`		SOF_TKN_INTEL_DMIC_FIFO_WORD_LENGTH'	STR($8)
`	}'
`}'
dnl PDM config for the number of active PDM controllers
$11
`SectionData."'N_DAI_CONFIG($9$10)`_pdm_data" {'
`	tuples "'N_DAI_CONFIG($9$10)`_pdm_tuples"'
`}'
`SectionData."'N_DAI_CONFIG($9$10)`_data" {'
`	tuples "'N_DAI_CONFIG($9$10)`_dmic_tuples"'

`}'
)

dnl DAI Config)
define(`N_DAI_CONFIG', `DAICONFIG.'$1)

dnl DAI_CONFIG(type, idx, link_id, name, ssp_config/dmic_config)
define(`DAI_CONFIG',
`SectionHWConfig."'$1$2`" {'
`'
`	id		"'$2`"'
`'
`	ifelse($1, `SSP', $5, `}')'
`ifelse($1, `DMIC', $5, `')'
`SectionVendorTuples."'N_DAI_CONFIG($1$2)`_tuples_str" {'
`	tokens "sof_dai_tokens"'
`	tuples."string" {'
`		SOF_TKN_DAI_TYPE'		STR($1)
`	}'
`}'
`SectionData."'N_DAI_CONFIG($1$2)`_data_str" {'
`	tuples "'N_DAI_CONFIG($1$2)`_tuples_str"'
`}'
`'
`SectionBE."'$4`" {'
`	id "'$3`"'
`	index "0"'
`	default_hw_conf_id	"'$2`"'
`'
`	hw_configs ['
`		"'$1$2`"'
`	]'
`	data ['
`		"'N_DAI_CONFIG($1$2)`_data"'
`		"'N_DAI_CONFIG($1$2)`_data_str"'
`ifelse($1, `DMIC',`		"'N_DAI_CONFIG($1$2)`_pdm_data"', `')'
`	]'
`}')

dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     frames, deadline, priority, core)
define(`DAI_ADD',
`undefine(`PIPELINE_ID')'
`undefine(`DAI_TYPE')'
`undefine(`DAI_INDEX')'
`undefine(`DAI_BE')'
`undefine(`DAI_BUF')'
`undefine(`DAI_PERIODS')'
`undefine(`DAI_FORMAT')'
`undefine(`SCHEDULE_FRAMES')'
`undefine(`SCHEDULE_DEADLINE')'
`undefine(`SCHEDULE_PRIORITY')'
`undefine(`SCHEDULE_CORE')'
`define(`PIPELINE_ID', $2)'
`define(`DAI_TYPE', STR($3))'
`define(`DAI_INDEX', STR($4))'
`define(`DAI_BE', $5)'
`define(`DAI_BUF', $6)'
`define(`DAI_NAME', $3$4)'
`define(`DAI_PERIODS', $7)'
`define(`DAI_FORMAT', $8)'
`define(`SCHEDULE_FRAMES', $9)'
`define(`SCHEDULE_DEADLINE', $10)'
`define(`SCHEDULE_PRIORITY', $11)'
`define(`SCHEDULE_CORE', $12)'
`include($1)'
)

divert(0)dnl
