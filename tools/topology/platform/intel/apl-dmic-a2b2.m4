# apl dmic settings for A2-B2
undefine(`ACHANNEL')
undefine(`AFORMAT')
undefine(`ARATE')
undefine(`APDM')

undefine(`BCHANNEL')
undefine(`BFORMAT')
undefine(`BRATE')
undefine(`BPDM')

define(`ACHANNEL', `2')
define(`AFORMAT', `s32le')
define(`ARATE', `48000')
define(`APDM', `STEREO_PDM0')

define(`BCHANNEL', `2')
define(`BFORMAT', `s32le')
define(`BRATE', `16000')
define(`BPDM', `STEREO_PDM1')
