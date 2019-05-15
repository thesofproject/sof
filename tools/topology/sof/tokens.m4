
#
# SOF Tokens for differentiation.
#
# Differentiation can be done at the platform and machine level.
#
# Tokens are GUIDs

# TODO: pre-process this with UAPI headers GNU cpp.


SectionVendorTokens."sof_buffer_tokens" {
	SOF_TKN_BUF_SIZE			"100"
	SOF_TKN_BUF_CAPS			"101"
}

SectionVendorTokens."sof_dai_tokens" {
# Token retired with ABI 3.2, do not use for new capabilities
#	SOF_TKN_DAI_DMAC_CONFIG			"153"
	SOF_TKN_DAI_TYPE			"154"
	SOF_TKN_DAI_INDEX			"155"
	SOF_TKN_DAI_DIRECTION			"156"
}

SectionVendorTokens."sof_sched_tokens" {
	SOF_TKN_SCHED_PERIOD			"200"
	SOF_TKN_SCHED_PRIORITY			"201"
	SOF_TKN_SCHED_MIPS			"202"
	SOF_TKN_SCHED_CORE			"203"
	SOF_TKN_SCHED_FRAMES			"204"
	SOF_TKN_SCHED_TIME_DOMAIN		"205"
}

SectionVendorTokens."sof_volume_tokens" {
	SOF_TKN_VOLUME_RAMP_STEP_TYPE		"250"
	SOF_TKN_VOLUME_RAMP_STEP_MS		"251"
}

SectionVendorTokens."sof_src_tokens" {
	SOF_TKN_SRC_RATE_IN			"300"
	SOF_TKN_SRC_RATE_OUT			"301"
}

SectionVendorTokens."sof_pcm_tokens" {
	SOF_TKN_PCM_DMAC_CONFIG			"353"
}

SectionVendorTokens."sof_comp_tokens" {
	SOF_TKN_COMP_PERIOD_SINK_COUNT		"400"
	SOF_TKN_COMP_PERIOD_SOURCE_COUNT	"401"
	SOF_TKN_COMP_FORMAT			"402"
# Token retired with ABI 3.2, do not use for new capabilities
#	SOF_TKN_COMP_PRELOAD_COUNT		"403"
}

SectionVendorTokens."sof_ssp_tokens" {
	SOF_TKN_INTEL_SSP_CLKS_CONTROL		"500"
	SOF_TKN_INTEL_SSP_MCLK_ID		"501"
	SOF_TKN_INTEL_SSP_SAMPLE_BITS		"502"
	SOF_TKN_INTEL_SSP_FRAME_PULSE_WIDTH 	"503"
	SOF_TKN_INTEL_SSP_QUIRKS		"504"
	SOF_TKN_INTEL_SSP_TDM_PADDING_PER_SLOT  "505"
	SOF_TKN_INTEL_SSP_BCLK_DELAY		"506"
}

SectionVendorTokens."sof_dmic_tokens" {
	SOF_TKN_INTEL_DMIC_DRIVER_VERSION	"600"
	SOF_TKN_INTEL_DMIC_CLK_MIN		"601"
	SOF_TKN_INTEL_DMIC_CLK_MAX		"602"
	SOF_TKN_INTEL_DMIC_DUTY_MIN		"603"
	SOF_TKN_INTEL_DMIC_DUTY_MAX		"604"
	SOF_TKN_INTEL_DMIC_NUM_PDM_ACTIVE	"605"
	SOF_TKN_INTEL_DMIC_SAMPLE_RATE		"608"
	SOF_TKN_INTEL_DMIC_FIFO_WORD_LENGTH	"609"
	SOF_TKN_INTEL_DMIC_UNMUTE_RAMP_TIME_MS	"610"
}

SectionVendorTokens."sof_dmic_pdm_tokens" {
	SOF_TKN_INTEL_DMIC_PDM_CTRL_ID		"700"
	SOF_TKN_INTEL_DMIC_PDM_MIC_A_Enable	"701"
	SOF_TKN_INTEL_DMIC_PDM_MIC_B_Enable	"702"
	SOF_TKN_INTEL_DMIC_PDM_POLARITY_A	"703"
	SOF_TKN_INTEL_DMIC_PDM_POLARITY_B	"704"
	SOF_TKN_INTEL_DMIC_PDM_CLK_EDGE		"705"
	SOF_TKN_INTEL_DMIC_PDM_SKEW		"706"
}

SectionVendorTokens."sof_tone_tokens" {
	SOF_TKN_TONE_SAMPLE_RATE		"800"
}

SectionVendorTokens."sof_process_tokens" {
	SOF_TKN_PROCESS_TYPE			"900"
}

SectionVendorTokens."sof_sai_tokens" {
	SOF_TKN_INTEL_SAI_CLKS_CONTROL		"1500"
	SOF_TKN_INTEL_SAI_MCLK_ID		"1501"
	SOF_TKN_INTEL_SAI_SAMPLE_BITS		"1502"
	SOF_TKN_INTEL_SAI_FRAME_PULSE_WIDTH 	"1503"
	SOF_TKN_INTEL_SAI_QUIRKS		"1504"
	SOF_TKN_INTEL_SAI_TDM_PADDING_PER_SLOT  "1505"
}
