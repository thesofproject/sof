#
# Broxton differentiation for pipelines and components
#

include(`memory.m4')

dnl Memory capabilities for diferent buffer types on Cannonlake
define(`PLATFORM_DAI_MEM_CAP',
	MEMCAPS(MEM_CAP_RAM, MEM_CAP_DMA, MEM_CAP_CACHE, MEM_CAP_HP))
define(`PLATFORM_HOST_MEM_CAP',
	MEMCAPS(MEM_CAP_RAM, MEM_CAP_DMA, MEM_CAP_CACHE, MEM_CAP_HP))
define(`PLATFORM_PASS_MEM_CAP',
	MEMCAPS(MEM_CAP_RAM, MEM_CAP_DMA, MEM_CAP_CACHE, MEM_CAP_HP))
define(`PLATFORM_COMP_MEM_CAP', MEMCAPS(MEM_CAP_RAM, MEM_CAP_CACHE))

# Low Latency PCM Configuration
# Low Latency PCM Configuration
W_VENDORTUPLES(pipe_ll_schedule_plat_tokens, sof_sched_tokens, LIST(`		', `SOF_TKN_SCHED_MIPS	"50000"'))

SectionData."pipe_ll_schedule_plat" {
	tuples "pipe_ll_schedule_plat_tokens"
}

# Media PCM Configuration
W_VENDORTUPLES(pipe_media_schedule_plat_tokens, sof_sched_tokens, LIST(`		', `SOF_TKN_SCHED_MIPS	"100000"'))

SectionData."pipe_media_schedule_plat" {
	tuples "pipe_media_schedule_plat_tokens"
}

# Tone Signal Generator Configuration
W_VENDORTUPLES(pipe_tone_schedule_plat_tokens, sof_sched_tokens, LIST(`		', `SOF_TKN_SCHED_MIPS	"200000"'))

SectionData."pipe_tone_schedule_plat" {
	tuples "pipe_tone_schedule_plat_tokens"
}

# DAI0 platform playback configuration
W_VENDORTUPLES(dai0p_plat_tokens, sof_dai_tokens, LIST(`		', `SOF_TKN_DAI_DMAC	"1"', `SOF_TKN_DAI_DMAC_CHAN	"0"'))

SectionData."dai0p_plat_conf" {
	tuples "dai0p_plat_tokens"
}

# DAI0 platform capture configuration
W_VENDORTUPLES(dai0c_plat_tokens, sof_dai_tokens, LIST(`		', `SOF_TKN_DAI_DMAC	"1"', `SOF_TKN_DAI_DMAC	"1"'))

SectionData."dai0c_plat_conf" {
	tuples "dai0c_plat_tokens"
}

# PCM platform configuration
W_VENDORTUPLES(pcm_plat_tokens, sof_dai_tokens, LIST(`		', `SOF_TKN_DAI_DMAC	PIPELINE_DMAC', `SOF_TKN_DAI_DMAC_CHAN	PIPELINE_DMAC_CHAN'))

SectionData."pcm_plat_conf" {
	tuples "pcm_plat_tokens"
}

# DAI schedule Configuration - scheduled by IRQ
W_VENDORTUPLES(pipe_dai_schedule_plat_tokens, sof_sched_tokens, LIST(`		', `SOF_TKN_SCHED_MIPS	"5000"'))

SectionData."pipe_dai_schedule_plat" {
	tuples "pipe_dai_schedule_plat_tokens"
}
