
list(APPEND TPLGS
# HDMI only topology with passthrough pipelines
"sof-hda-generic\;sof-hda-generic-idisp\;DEEPBUFFER_FW_DMA_MS=100,\
DEEPBUFFER_D0I3_COMPATIBLE=true"
# HDA topology with mixer-based pipelines for HDA and passthrough pipelines for HDMI
"sof-hda-generic\;sof-hda-generic\;HDA_CONFIG=mix,DEEPBUFFER_FW_DMA_MS=100,\
DEEPBUFFER_D0I3_COMPATIBLE=true"
)
