divert(-1)

# list of generic scheduling time domains
define(`SCHEDULE_TIME_DOMAIN_DMA', 0)
define(`SCHEDULE_TIME_DOMAIN_TIMER', 1)

# default number of DAI periods
define(`DAI_DEFAULT_PERIODS', 2)

# default stereo channel map
ifdef(`VOLUME_CHANNEL_MAP', , `define(`VOLUME_CHANNEL_MAP', LIST(`	',
								KCONTROL_CHANNEL(FL, 1, 0),
								KCONTROL_CHANNEL(FR, 1, 1)))')

ifdef(`SWITCH_CHANNEL_MAP', , `define(`SWITCH_CHANNEL_MAP', LIST(`	',
								KCONTROL_CHANNEL(FL, 2, 0),
								KCONTROL_CHANNEL(FR, 2, 1)))')

dnl Define macro for pipeline widget

dnl Pipeline name)
define(`N_PIPELINE', `PIPELINE.'PIPELINE_ID`.'$1)

dnl W_PIPELINE(stream, period, priority, core, initiator, platform)
define(`W_PIPELINE',
`SectionVendorTuples."'N_PIPELINE($1)`_tuples" {'
`	tokens "sof_sched_tokens"'
`	tuples."word" {'
`		SOF_TKN_SCHED_PERIOD'		STR($2)
`		SOF_TKN_SCHED_PRIORITY'		STR($3)
`		SOF_TKN_SCHED_CORE'		STR($4)
`		SOF_TKN_SCHED_FRAMES'		"0"
`		SOF_TKN_SCHED_TIME_DOMAIN'	STR($5)
`		SOF_TKN_SCHED_DYNAMIC_PIPELINE'	ifdef(`DYNAMIC', "1", ifelse(DYNAMIC_PIPE, `1', "1", "0"))
`	}'
`}'
`SectionData."'N_PIPELINE($1)`_data" {'
`	tuples "'N_PIPELINE($1)`_tuples"'
`}'
`SectionWidget."'N_PIPELINE($1)`" {'
`	index "'PIPELINE_ID`"'
`	type "scheduler"'
`	no_pm "true"'
`	stream_name "'$1`"'
`	data ['
`		"'N_PIPELINE($1)`_data"'
`		"'$6`"'
`	]'
`}')

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate,
dnl     time_domain, sched_comp, dynamic)
define(`PIPELINE_PCM_ADD',
`ifelse(eval(`$# > 10'), `1',
`undefine(`PCM_ID')'
`undefine(`PIPELINE_ID')'
`undefine(`PIPELINE_CHANNELS')'
`undefine(`PIPELINE_FORMAT')'
`undefine(`SCHEDULE_PERIOD')'
`undefine(`SCHEDULE_PRIORITY')'
`undefine(`SCHEDULE_CORE')'
`undefine(`PCM_MIN_RATE')'
`undefine(`PCM_MAX_RATE')'
`undefine(`PIPELINE_RATE')'
`undefine(`SCHEDULE_TIME_DOMAIN')'
`undefine(`DAI_FORMAT')'
`undefine(`SCHED_COMP')'
`undefine(`DYNAMIC_PIPE')'
`define(`PIPELINE_ID', $2)'
`define(`PCM_ID', $3)'
`define(`PIPELINE_CHANNELS', $4)'
`define(`PIPELINE_FORMAT', $5)'
`define(`SCHEDULE_PERIOD', $6)'
`define(`SCHEDULE_PRIORITY', $7)'
`define(`SCHEDULE_CORE', $8)'
`define(`PCM_MIN_RATE', $9)'
`define(`PCM_MAX_RATE', $10)'
`define(`PIPELINE_RATE', $11)'
`define(`SCHEDULE_TIME_DOMAIN', $12)'
`define(`DAI_FORMAT', $5)'
`define(`SCHED_COMP', $13)'
`define(`DYNAMIC_PIPE', $14)'
`ifdef(`DAI_PERIODS', `', `define(`DAI_PERIODS', DAI_DEFAULT_PERIODS)')'
`include($1)'
`DEBUG_PCM_ADD($1, $3)'
,`fatal_error(`Invalid parameters ($#) to PIPELINE_PCM_ADD')')'
)

dnl PIPELINE_PCM_DAI_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     dai type, dai_index, dai format,
dnl     periods, time_domain)
define(`PIPELINE_PCM_DAI_ADD',
`ifelse(`$#', `15',
`undefine(`PCM_ID')'
`undefine(`PIPELINE_ID')'
`undefine(`PIPELINE_CHANNELS')'
`undefine(`PIPELINE_FORMAT')'
`undefine(`SCHEDULE_PERIOD')'
`undefine(`SCHEDULE_PRIORITY')'
`undefine(`SCHEDULE_CORE')'
`undefine(`SCHEDULE_TIME_DOMAIN')'
`undefine(`DAI_TYPE')'
`undefine(`DAI_INDEX')'
`undefine(`DAI_FORMAT')'
`undefine(`DAI_PERIODS')'
`undefine(`PCM_MIN_RATE')'
`undefine(`PCM_MAX_RATE')'
`undefine(`PIPELINE_RATE')'
`define(`PIPELINE_ID', $2)'
`define(`PCM_ID', $3)'
`define(`PIPELINE_CHANNELS', $4)'
`define(`PIPELINE_FORMAT', $5)'
`define(`SCHEDULE_PERIOD', $6)'
`define(`SCHEDULE_PRIORITY', $7)'
`define(`SCHEDULE_CORE', $8)'
`define(`SCHEDULE_TIME_DOMAIN', $16)'
`define(`DAI_TYPE', STR($9))'
`define(`DAI_INDEX', STR($10))'
`define(`DAI_FORMAT', $11)'
`define(`DAI_PERIODS', $12)'
`define(`DAI_NAME', $9$10)'
`define(`PCM_MIN_RATE', $13)'
`define(`PCM_MAX_RATE', $14)'
`define(`PIPELINE_RATE', $15)'
`include($1)'
,`fatal_error(`Invalid parameters ($#) to PIPELINE_PCM_DAI_ADD')')'
)

dnl PIPELINE_ADD(pipeline,
dnl     pipe id, max channels, format,
dnl     period, priority, core,
dnl     sched_comp, time_domain,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate, dynamic)
define(`PIPELINE_ADD',
`ifelse(`$#', `12',
`undefine(`PIPELINE_ID')'
`undefine(`PIPELINE_CHANNELS')'
`undefine(`PIPELINE_FORMAT')'
`undefine(`SCHEDULE_PERIOD')'
`undefine(`SCHEDULE_PRIORITY')'
`undefine(`SCHEDULE_CORE')'
`undefine(`SCHEDULE_TIME_DOMAIN')'
`undefine(`PCM_MIN_RATE')'
`undefine(`PCM_MAX_RATE')'
`undefine(`PIPELINE_RATE')'
`undefine(`DYNAMIC_PIPE')'
`define(`PIPELINE_ID', $2)'
`define(`PIPELINE_CHANNELS', $3)'
`define(`PIPELINE_FORMAT', $4)'
`define(`SCHEDULE_PERIOD', $5)'
`define(`SCHEDULE_PRIORITY', $6)'
`define(`SCHEDULE_CORE', $7)'
`define(`SCHEDULE_TIME_DOMAIN', $9)'
`define(`SCHED_COMP', $8)'
`define(`PCM_MIN_RATE', $10)'
`define(`PCM_MAX_RATE', $11)'
`define(`PIPELINE_RATE', $12)'
`define(`DYNAMIC_PIPE', $13)'
`include($1)'
,`fatal_error(`Invalid parameters ($#) to PIPELINE_ADD')')'
)

divert(0)dnl
