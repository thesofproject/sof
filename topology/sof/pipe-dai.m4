# DAI connector

# Include topology builder
include(`local.m4')

#
# DAI definitions
#
W_DAI_OUT(DAI_SNAME, dai0p_plat_conf)
W_DAI_IN(DAI_SNAME, dai0c_plat_conf)

#D_DAI(0, 1, 1)

#
# Graph connections to pipelines

SectionGraph.STR(DAI_NAME) {
	index "0"

	lines [
		dapm(N_DAI_IN, OUT_BUF)
		dapm(IN_BUF, N_DAI_OUT)
	]
}
