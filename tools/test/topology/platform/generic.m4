#
# Generic test platform values for pipelines and components
#

undefine(`SSP_MCLK_RATE')
define(`SSP_MCLK_RATE', `19200000')

undefine(`SSP1_BCLK')
define(`SSP1_BCLK', `1536000')

undefine(`SSP_BCLK')
define(`SSP_BCLK', `1920000')

undefine(`SSP_FSYNC')
define(`SSP_FSYNC', `48000')

define(`SSP1_VALID_BITS_STR', `s16le')

undefine(`SSP_BITS_WIDTH')
define(`SSP_BITS_WIDTH', `20')

undefine(`SSP1_VALID_BITS')
define(`SSP1_VALID_BITS', `16')

undefine(`SSP_VALID_BITS')
define(`SSP_VALID_BITS', `16')

undefine(`MCLK_ID')
define(`MCLK_ID', `1')

include(`memory.m4')

dnl Memory capabilities for different buffer types
define(`PLATFORM_DAI_MEM_CAP',
	MEMCAPS(MEM_CAP_RAM, MEM_CAP_DMA, MEM_CAP_CACHE, MEM_CAP_HP))
define(`PLATFORM_HOST_MEM_CAP',
	MEMCAPS(MEM_CAP_RAM, MEM_CAP_DMA, MEM_CAP_CACHE, MEM_CAP_HP))
define(`PLATFORM_PASS_MEM_CAP',
	MEMCAPS(MEM_CAP_RAM, MEM_CAP_DMA, MEM_CAP_CACHE, MEM_CAP_HP))
define(`PLATFORM_COMP_MEM_CAP', MEMCAPS(MEM_CAP_RAM, MEM_CAP_CACHE))

# Low Latency PCM Configuration
W_VENDORTUPLES(pipe_ll_schedule_plat_tokens, sof_sched_tokens, LIST(`		', `SOF_TKN_SCHED_MIPS	"50000"'))

W_DATA(pipe_ll_schedule_plat, pipe_ll_schedule_plat_tokens)

# Media PCM Configuration
W_VENDORTUPLES(pipe_media_schedule_plat_tokens, sof_sched_tokens, LIST(`		', `SOF_TKN_SCHED_MIPS	"100000"'))

W_DATA(pipe_media_schedule_plat, pipe_media_schedule_plat_tokens)

# Tone Signal Generator Configuration
W_VENDORTUPLES(pipe_tone_schedule_plat_tokens, sof_sched_tokens, LIST(`		', `SOF_TKN_SCHED_MIPS	"200000"'))

W_DATA(pipe_tone_schedule_plat, pipe_tone_schedule_plat_tokens)

# DAI schedule Configuration - scheduled by IRQ
W_VENDORTUPLES(pipe_dai_schedule_plat_tokens, sof_sched_tokens, LIST(`		', `SOF_TKN_SCHED_MIPS	"5000"'))

W_DATA(pipe_dai_schedule_plat, pipe_dai_schedule_plat_tokens)
