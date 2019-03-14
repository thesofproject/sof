divert(-1)

dnl Define macro for pipeline widget

dnl Pipeline name)
define(`N_PIPELINE', `PIPELINE.'PIPELINE_ID`.'$1)

dnl W_PIPELINE(stream, deadline, priority, frames, core, timer, platform)
define(`W_PIPELINE',
`SectionVendorTuples."'N_PIPELINE($1)`_tuples" {'
`	tokens "sof_sched_tokens"'
`	tuples."word" {'
`		SOF_TKN_SCHED_DEADLINE'		STR($2)
`		SOF_TKN_SCHED_PRIORITY'		STR($3)
`		SOF_TKN_SCHED_CORE'		STR($5)
`		SOF_TKN_SCHED_FRAMES'		STR($4)
`		SOF_TKN_SCHED_TIMER'		STR($6)
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
`		"'$7`"'
`	]'
`}')

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     frames, deadline, priority, core, timer)
define(`PIPELINE_PCM_ADD',
`undefine(`PCM_ID')'
`undefine(`PIPELINE_ID')'
`undefine(`PIPELINE_CHANNELS')'
`undefine(`PIPELINE_FORMAT')'
`undefine(`SCHEDULE_FRAMES')'
`undefine(`SCHEDULE_DEADLINE')'
`undefine(`SCHEDULE_PRIORITY')'
`undefine(`SCHEDULE_CORE')'
`undefine(`SCHEDULE_TIMER')'
`define(`PIPELINE_ID', $2)'
`define(`PCM_ID', $3)'
`define(`PIPELINE_CHANNELS', $4)'
`define(`PIPELINE_FORMAT', $5)'
`define(`SCHEDULE_FRAMES', $6)'
`define(`SCHEDULE_DEADLINE', $7)'
`define(`SCHEDULE_PRIORITY', $8)'
`define(`SCHEDULE_CORE', $9)'
`define(`SCHEDULE_TIMER', $10)'
`define(`DAI_FORMAT', $5)'
`include($1)'
`DEBUG_PCM_ADD($1, $3)'
)

dnl PIPELINE_PCM_DAI_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     frames, deadline, priority, core,
dnl     dai type, dai_index, dai format,
dnl     periods, timer)
define(`PIPELINE_PCM_DAI_ADD',
`undefine(`PCM_ID')'
`undefine(`PIPELINE_ID')'
`undefine(`PIPELINE_CHANNELS')'
`undefine(`PIPELINE_FORMAT')'
`undefine(`SCHEDULE_FRAMES')'
`undefine(`SCHEDULE_DEADLINE')'
`undefine(`SCHEDULE_PRIORITY')'
`undefine(`SCHEDULE_CORE')'
`undefine(`SCHEDULE_TIMER')'
`undefine(`DAI_TYPE')'
`undefine(`DAI_INDEX')'
`undefine(`DAI_FORMAT')'
`undefine(`DAI_PERIODS')'
`define(`PIPELINE_ID', $2)'
`define(`PCM_ID', $3)'
`define(`PIPELINE_CHANNELS', $4)'
`define(`PIPELINE_FORMAT', $5)'
`define(`SCHEDULE_FRAMES', $6)'
`define(`SCHEDULE_DEADLINE', $7)'
`define(`SCHEDULE_PRIORITY', $8)'
`define(`SCHEDULE_CORE', $9)'
`define(`SCHEDULE_TIMER', $14)'
`define(`DAI_TYPE', STR($10))'
`define(`DAI_INDEX', STR($11))'
`define(`DAI_FORMAT', $12)'
`define(`DAI_PERIODS', $13)'
`define(`DAI_NAME', $10$11)'
`include($1)'
)

dnl PIPELINE_ADD(pipeline,
dnl     pipe id, max channels, format,
dnl     frames, deadline, priority, core,
dnl     sched_comp, timer)
define(`PIPELINE_ADD',
`undefine(`PIPELINE_ID')'
`undefine(`PIPELINE_CHANNELS')'
`undefine(`PIPELINE_FORMAT')'
`undefine(`SCHEDULE_FRAMES')'
`undefine(`SCHEDULE_DEADLINE')'
`undefine(`SCHEDULE_PRIORITY')'
`undefine(`SCHEDULE_CORE')'
`undefine(`SCHEDULE_TIMER')'
`define(`PIPELINE_ID', $2)'
`define(`PIPELINE_CHANNELS', $3)'
`define(`PIPELINE_FORMAT', $4)'
`define(`SCHEDULE_FRAMES', $5)'
`define(`SCHEDULE_DEADLINE', $6)'
`define(`SCHEDULE_PRIORITY', $7)'
`define(`SCHEDULE_CORE', $8)'
`define(`SCHEDULE_TIMER', $10)'
`define(`SCHED_COMP', $9)'
`include($1)'
)

divert(0)dnl
