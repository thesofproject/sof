#
# Icelake differentiation for pipelines and components
#

include(`memory.m4')

dnl Memory capabilities for different buffer types on Icelake
define(`PLATFORM_DAI_MEM_CAP',
	MEMCAPS(MEM_CAP_RAM, MEM_CAP_DMA, MEM_CAP_CACHE, MEM_CAP_HP))
define(`PLATFORM_HOST_MEM_CAP',
	MEMCAPS(MEM_CAP_RAM, MEM_CAP_DMA, MEM_CAP_CACHE, MEM_CAP_HP))
define(`PLATFORM_PASS_MEM_CAP',
	MEMCAPS(MEM_CAP_RAM, MEM_CAP_DMA, MEM_CAP_CACHE, MEM_CAP_HP))
define(`PLATFORM_COMP_MEM_CAP', MEMCAPS(MEM_CAP_RAM, MEM_CAP_CACHE))

# Low Latency PCM Configuration
W_VENDORTUPLES(pipe_ll_schedule_plat_tokens, sof_sched_tokens,
		LIST(`		', `SOF_TKN_SCHED_MIPS	"50000"'))

W_DATA(pipe_ll_schedule_plat, pipe_ll_schedule_plat_tokens)

# Media PCM Configuration
W_VENDORTUPLES(pipe_media_schedule_plat_tokens, sof_sched_tokens,
		LIST(`		', `SOF_TKN_SCHED_MIPS	"100000"'))

W_DATA(pipe_media_schedule_plat, pipe_media_schedule_plat_tokens)

# Tone Signal Generator Configuration
W_VENDORTUPLES(pipe_tone_schedule_plat_tokens, sof_sched_tokens,
		LIST(`		', `SOF_TKN_SCHED_MIPS	"200000"'))

W_DATA(pipe_tone_schedule_plat, pipe_tone_schedule_plat_tokens)

# DAI schedule Configuration - scheduled by IRQ
W_VENDORTUPLES(pipe_dai_schedule_plat_tokens, sof_sched_tokens,
		LIST(`		', `SOF_TKN_SCHED_MIPS	"5000"'))

W_DATA(pipe_dai_schedule_plat, pipe_dai_schedule_plat_tokens)

#SSP setting for ICL platform
undefine(`SSP_INDEX')
define(`SSP_INDEX', 0)

undefine(`SSP_NAME')
define(`SSP_NAME', `SSP0-Codec')

undefine(`SSP_MCLK_RATE')
define(`SSP_MCLK_RATE', `19200000')



#--------- SSP1 --------------
#SSP setting for CML platform
define(`SSP1_INDEX', 1)
define(`SSP1_NAME', `SSP1-Codec')
define(`SSP1_MCLK_RATE', `24000000')

ifelse(SOF_ABI_VERSION_3_9_OR_GRT, `1',
    define(`SSP1_VALID_BITS', 24),
    define(`SSP1_VALID_BITS', 16))

# playback DAI is SSP1 using 2 periods
# Buffers use s16le format, with 48 frame per 1000us on core 0 with priority 0
# With m/n divider available we can support 24 bit playback
ifelse(SOF_ABI_VERSION_3_9_OR_GRT, `1',
    define(`SSP1_VALID_BITS_STR', s24le),
    define(`SSP1_VALID_BITS_STR', s16le))

ifelse(SOF_ABI_VERSION_3_9_OR_GRT, `1',
    define(`SSP1_BCLK', 2304000),
    define(`SSP1_BCLK', 1500000))

ifelse(SOF_ABI_VERSION_3_9_OR_GRT, `1',
    define(`SSP1_FSYNC', 48000),
    define(`SSP1_FSYNC', 46875))


#SSP 1 (ID: 6)
#Use BCLK delay in SSP_CONFIG_DATA only on supporting version
ifelse(SOF_ABI_VERSION_3_9_OR_GRT, `1',
    define(`SET_SSP1_CONFIG_DATA', SSP_CONFIG_DATA(SSP, 1, SSP1_VALID_BITS, 0, 0, 10)),
    define(`SET_SSP1_CONFIG_DATA', SSP_CONFIG_DATA(SSP, 1, SSP1_VALID_BITS)))

include(`platform/intel/dmic.m4')
