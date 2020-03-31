# apl dmic settings for A2-B4
undefine(`ACHANNEL')
undefine(`AFORMAT')
undefine(`ARATE')
undefine(`APDM')

undefine(`BCHANNEL')
undefine(`BFORMAT')
undefine(`BRATE')
undefine(`BPDM')

define(`ACHANNEL', `2')
define(`AFORMAT', `s16le')
define(`ARATE', `48000')
define(`APDM', `STEREO_PDM0')

define(`BCHANNEL', `4')
define(`BFORMAT', `s32le')
define(`BRATE', `16000')
define(`BPDM', `FOUR_CH_PDM0_PDM1')
