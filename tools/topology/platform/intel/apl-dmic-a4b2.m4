# apl dmic settings for A4-B2
undefine(`ACHANNEL')
undefine(`AFORMAT')
undefine(`ARATE')
undefine(`APDM')

undefine(`BCHANNEL')
undefine(`BFORMAT')
undefine(`BRATE')
undefine(`BPDM')

define(`ACHANNEL', `4')
define(`AFORMAT', `s16le')
define(`ARATE', `48000')
define(`APDM', `FOUR_CH_PDM0_PDM1')

define(`BCHANNEL', `2')
define(`BFORMAT', `s16le')
define(`BRATE', `16000')
define(`BPDM', `STEREO_PDM0')
