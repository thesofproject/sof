divert(-1)

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     frames, deadline, priority, core, dmac, dmac_chan)
define(`PIPELINE_PCM_ADD',
`undefine(`PCM_ID')'
`undefine(`PIPELINE_ID')'
`undefine(`PIPELINE_CHANNELS')'
`undefine(`PIPELINE_FORMAT')'
`undefine(`SCHEDULE_FRAMES')'
`undefine(`SCHEDULE_DEADLINE')'
`undefine(`SCHEDULE_PRIORITY')'
`undefine(`SCHEDULE_CORE')'
`undefine(`PIPELINE_DMAC')'
`undefine(`PIPELINE_DMAC_CHAN')'
`define(`PIPELINE_ID', $2)'
`define(`PCM_ID', $3)'
`define(`PIPELINE_CHANNELS', $4)'
`define(`PIPELINE_FORMAT', $5)'
`define(`SCHEDULE_FRAMES', $6)'
`define(`SCHEDULE_DEADLINE', $7)'
`define(`SCHEDULE_PRIORITY', $8)'
`define(`SCHEDULE_CORE', $9)'
`define(`PIPELINE_DMAC', $10)'
`define(`PIPELINE_DMAC_CHAN', $11)'
`include($1)'
)

dnl PIPELINE_PCM_DAI_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     frames, deadline, priority, core, dmac, dmac_chan,
dnl     dai type, dai_index, dai format, periods)
define(`PIPELINE_PCM_DAI_ADD',
`undefine(`PCM_ID')'
`undefine(`PIPELINE_ID')'
`undefine(`PIPELINE_CHANNELS')'
`undefine(`PIPELINE_FORMAT')'
`undefine(`SCHEDULE_FRAMES')'
`undefine(`SCHEDULE_DEADLINE')'
`undefine(`SCHEDULE_PRIORITY')'
`undefine(`SCHEDULE_CORE')'
`undefine(`PIPELINE_DMAC')'
`undefine(`PIPELINE_DMAC_CHAN')'
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
`define(`PIPELINE_DMAC', $10)'
`define(`PIPELINE_DMAC_CHAN', $11)'
`define(`DAI_TYPE', STR($12))'
`define(`DAI_INDEX', STR($13))'
`define(`DAI_FORMAT', $14)'
`define(`DAI_PERIODS', $15)'
`define(`DAI_NAME', $12$13)'
`include($1)'
)

dnl PIPELINE_ADD(pipeline,
dnl     pipe id, max channels, format,
dnl     frames, deadline, priority, core)
define(`PIPELINE_ADD',
`undefine(`PIPELINE_ID')'
`undefine(`PIPELINE_CHANNELS')'
`undefine(`PIPELINE_FORMAT')'
`undefine(`SCHEDULE_FRAMES')'
`undefine(`SCHEDULE_DEADLINE')'
`undefine(`SCHEDULE_PRIORITY')'
`undefine(`SCHEDULE_CORE')'
`define(`PIPELINE_ID', $2)'
`define(`PIPELINE_CHANNELS', $3)'
`define(`PIPELINE_FORMAT', $4)'
`define(`SCHEDULE_FRAMES', $5)'
`define(`SCHEDULE_DEADLINE', $6)'
`define(`SCHEDULE_PRIORITY', $7)'
`define(`SCHEDULE_CORE', $8)'
`include($1)'
)

dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index,
dnl     buffer, periods, format,
dnl     frames, deadline, priority, core)
define(`DAI_ADD',
`undefine(`PIPELINE_ID')'
`undefine(`DAI_TYPE')'
`undefine(`DAI_INDEX')'
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
`define(`DAI_BUF', $5)'
`define(`DAI_NAME', $3$4)'
`define(`DAI_PERIODS', $6)'
`define(`DAI_FORMAT', $7)'
`define(`SCHEDULE_FRAMES', $8)'
`define(`SCHEDULE_DEADLINE', $9)'
`define(`SCHEDULE_PRIORITY', $10)'
`define(`SCHEDULE_CORE', $11)'
`include($1)'
)

divert(0)dnl
