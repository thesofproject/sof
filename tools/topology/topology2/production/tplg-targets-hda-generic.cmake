
list(APPEND TPLGS
# HDMI only topology with passthrough pipelines
"sof-hda-generic\;sof-hda-generic-idisp\;DEEPBUFFER_FW_DMA_MS=100,\
DEEPBUFFER_D0I3_COMPATIBLE=true"
# HDA topology with mixer-based pipelines for HDA and passthrough pipelines for HDMI
"sof-hda-generic\;sof-hda-generic\;HDA_CONFIG=mix,DEEPBUFFER_FW_DMA_MS=100,\
DEEPBUFFER_D0I3_COMPATIBLE=true"

# HDA topology with mixer-based pipelines for HDA and
# passthrough pipelines for HDMI and
# 2 or 4 DMIC, no NHLT blob included in topology
"sof-hda-generic\;sof-hda-generic-2ch\;HDA_CONFIG=mix,NUM_DMICS=2,\
DEEPBUFFER_FW_DMA_MS=100,DEEPBUFFER_D0I3_COMPATIBLE=true"
"sof-hda-generic\;sof-hda-generic-4ch\;HDA_CONFIG=mix,NUM_DMICS=4,\
DEEPBUFFER_FW_DMA_MS=100,DEEPBUFFER_D0I3_COMPATIBLE=true,\
PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1"
)
