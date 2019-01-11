#
# Baytrail differentiation for pipelines and components
#

include(`memory.m4')

dnl Memory capabilities for diferent buffer types on Baytrail
define(`PLATFORM_DAI_MEM_CAP', MEMCAPS(MEM_CAP_RAM, MEM_CAP_DMA, MEM_CAP_CACHE))
define(`PLATFORM_HOST_MEM_CAP', MEMCAPS(MEM_CAP_RAM, MEM_CAP_DMA, MEM_CAP_CACHE))
define(`PLATFORM_PASS_MEM_CAP', MEMCAPS(MEM_CAP_RAM, MEM_CAP_DMA, MEM_CAP_CACHE))
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

VIRTUAL_WIDGET(codec_out0, output, 1)
VIRTUAL_WIDGET(codec_out1, output, 2)
VIRTUAL_WIDGET(codec_in0, input, 3)
VIRTUAL_WIDGET(codec_in1, input, 4)
