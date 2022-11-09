#
# ACPX differentiation for pipelines and components
#

include(`memory.m4')

# PCM Configuration
W_VENDORTUPLES(pipe_ll_schedule_plat_tokens, sof_sched_tokens, LIST(`		', `SOF_TKN_SCHED_MIPS	"50000"'))
W_DATA(pipe_ll_schedule_plat, pipe_ll_schedule_plat_tokens)

# DAI schedule Configuration - scheduled by IRQ
W_VENDORTUPLES(pipe_dai_schedule_plat_tokens, sof_sched_tokens, LIST(`		', `SOF_TKN_SCHED_MIPS	"5000"'))
W_DATA(pipe_dai_schedule_plat, pipe_dai_schedule_plat_tokens)

# Media PCM Configuration
W_VENDORTUPLES(pipe_media_schedule_plat_tokens, sof_sched_tokens, LIST(`		', `SOF_TKN_SCHED_MIPS	"5000"'))

W_DATA(pipe_media_schedule_plat, pipe_media_schedule_plat_tokens)

# Media PCM Configuration
W_VENDORTUPLES(pipe_media_schedule_plat_dmic_tokens, sof_sched_tokens, LIST(`		', `SOF_TKN_SCHED_MIPS	"5000"'))

W_DATA(pipe_media_schedule_plat_dmic, pipe_media_schedule_plat_dmic_tokens)


# Tone Signal Generator Configuration
W_VENDORTUPLES(pipe_tone_schedule_plat_tokens, sof_sched_tokens,
		LIST(`		', `SOF_TKN_SCHED_MIPS	"200000"'))

W_DATA(pipe_tone_schedule_plat, pipe_tone_schedule_plat_tokens)

# DAI schedule Configuration - scheduled by IRQ
W_VENDORTUPLES(pipe_dai_schedule_plat_tokens, sof_sched_tokens,
		LIST(`		', `SOF_TKN_SCHED_MIPS	"5000"'))

W_DATA(pipe_dai_schedule_plat, pipe_dai_schedule_plat_tokens)
