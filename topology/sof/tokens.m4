
#
# SOF Tokens for differentiation.
#
# Differentiation can be done at the platform and machine level.
#
# Tokens are GUIDs

# TODO: pre-process this with UAPI headers GNU cpp.


SectionVendorTokens."sof_buffer_tokens" {
	SOF_TKN_BUF_SIZE			"100"
	SOF_TKN_BUF_PRELOAD			"101"
}

SectionVendorTokens."sof_dai_tokens" {
	SOF_TKN_DAI_DMAC			"151"
	SOF_TKN_DAI_DMAC_CHAN			"152"
	SOF_TKN_DAI_DMAC_CONFIG			"153"
}

SectionVendorTokens."sof_sched_tokens" {
	SOF_TKN_SCHED_DEADLINE			"200"
	SOF_TKN_SCHED_PRIORITY			"201"
	SOF_TKN_SCHED_MIPS			"202"
	SOF_TKN_SCHED_CORE			"203"
}

SectionVendorTokens."sof_volume_tokens" {
	SOF_TKN_VOLUME_RAMP_STEP_TYPE		"250"
	SOF_TKN_VOLUME_RAMP_STEP_MS		"251"
}

SectionVendorTokens."sof_src_tokens" {
	SOF_TKN_SRC_RATE_IN			"300"
	SOF_TKN_SRC_RATE_OUT			"301"
}
