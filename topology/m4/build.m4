#"PIPELINE_PCM_ADD(pipeline, id, pcm, pcm_buf, int_buf, frames, deadline)
define(`PIPELINE_PCM_ADD',
`undefine(`PCM_ID')'
`undefine(`PIPELINE_ID')'
`undefine(`BUF_PCM_SIZE')'
`undefine(`BUF_INT_SIZE')'
`undefine(`SCHEDULE_FRAMES')'
`undefine(`SCHEDULE_DEADLINE')'
`define(`PCM_ID', $3)'
`define(`PIPELINE_ID', $2)'
`define(`BUF_PCM_SIZE', STR($4))'
`define(`BUF_INT_SIZE', STR($5))'
`define(`SCHEDULE_FRAMES', STR($6))'
`define(`SCHEDULE_DEADLINE', STR($7))'
`include($1)'
)

#PIPELINE_ADD(pipeline, id), int_buf, frames, deadline)
define(`PIPELINE_ADD',
`undefine(`PIPELINE_ID')'
`undefine(`BUF_INT_SIZE')'
`undefine(`SCHEDULE_FRAMES')'
`undefine(`SCHEDULE_DEADLINE')'
`define(`PIPELINE_ID', $2)'
`define(`BUF_INT_SIZE', STR($3))'
`define(`SCHEDULE_FRAMES', STR($4))'
`define(`SCHEDULE_DEADLINE', STR($5))'
`include($1)'
)

#DAI_ADD(pipeline, dai_name, stream_name, win, wout))
define(`DAI_ADD',
`undefine(`PIPELINE_ID')'
`undefine(`DAI_NAME')'
`undefine(`DAI_SNAME')'
`undefine(`IN_BUF')'
`undefine(`OUT_BUF')'
`define(`PIPELINE_ID', 0)'
`define(`DAI_NAME', $2)'
`define(`DAI_SNAME', $3)'
`define(`IN_BUF', $4)'
`define(`OUT_BUF', $5)'
`include($1)'
)
