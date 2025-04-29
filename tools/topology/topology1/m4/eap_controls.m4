
# EAP supported Audio Processing algorithms
CONTROLENUM_LIST(DEF_EAP_CFG_VALUES,
	LIST(`	', `"Off"', `"AutoVolumeLeveler"', `"ConcertSound"', `"LoudnessMaximiser"', `"MusicEnhancerRMSLimiter"', `"VoiceEnhancer"'))

# TDFB enum control
C_CONTROLENUM(DEF_EAP_CFG, PIPELINE_ID,
	DEF_EAP_CFG_VALUES,
	LIST(`	', ENUM_CHANNEL(FC, 3, 0)),
	CONTROLENUM_OPS(enum,
	257 binds the mixer control to enum get/put handlers,
	257, 257))
