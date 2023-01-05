# SPDX-License-Identifier: BSD-3-Clause

# Array of "input-file-name;output-file-name;comma separated pre-processor variables"
set(TPLGS
# CAVS SDW topology with passthrough pipelines
"cavs-sdw\;cavs-sdw\;DEEPBUFFER_FW_DMA_MS=100"

# CAVS SDW with SRC gain and mixer support
"cavs-sdw-src-gain-mixin\;cavs-sdw-src-gain-mixin"

# SDW + HDMI topology with passthrough pipelines
"cavs-sdw\;cavs-sdw-hdmi\;DEEPBUFFER_FW_DMA_MS=100"

# CAVS SSP topology for TGL
"cavs-nocodec\;sof-tgl-nocodec\;NUM_DMICS=4,PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-tgl-nocodec.bin,DEEPBUFFER_FW_DMA_MS=100"
"cavs-nocodec\;sof-adl-nocodec\;NUM_DMICS=4,PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-adl-nocodec.bin,DEEPBUFFER_FW_DMA_MS=100"

# SDW topology for MTL
"cavs-sdw\;mtl-sdw\;NUM_HDMIS=0"

# SDW + HDMI topology for MTL
"cavs-sdw\;mtl-sdw-hdmi\;PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-mtl-sdw-hdmi.bin"

# SSP topology for MTL
"cavs-nocodec\;sof-mtl-nocodec\;PLATFORM=mtl,NUM_DMICS=2,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-mtl-nocodec.bin,DEEPBUFFER_FW_DMA_MS=100"

# CAVS HDA topology with mixer-based efx eq pipelines for HDA and passthrough pipelines for HDMI
"sof-hda-generic\;sof-hda-efx-generic\;HDA_CONFIG=efx,USE_CHAIN_DMA=true,DEEPBUFFER_FW_DMA_MS=100,\
EFX_FIR_PARAMS=passthrough,EFX_IIR_PARAMS=passthrough"
"sof-hda-generic\;sof-hda-efx-generic-2ch\;\
HDA_CONFIG=efx,NUM_DMICS=2,PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-hda-fir-generic-2ch.bin,\
USE_CHAIN_DMA=true,DEEPBUFFER_FW_DMA_MS=100,EFX_FIR_PARAMS=passthrough,EFX_IIR_PARAMS=passthrough"
"sof-hda-generic\;sof-hda-efx-generic-4ch\;\
HDA_CONFIG=efx,NUM_DMICS=4,PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-hda-efx-generic-4ch.bin,USE_CHAIN_DMA=true,\
DEEPBUFFER_FW_DMA_MS=100,EFX_FIR_PARAMS=passthrough,EFX_IIR_PARAMS=passthrough"
)
add_custom_target(topology2_dev)

foreach(tplg ${TPLGS})
	set(defines "")
	list(LENGTH tplg length)
	list(GET tplg 0 input)
	list(GET tplg 1 output)

	math(EXPR last_index "${length}-1")

	# Get the pre-processor definitions from the 3rd item in the list for each topology
	# ex: "hda-generic\;hda-generic-2ch\;HDA_CONFIG=gain,DMIC_CH=2", "defines" would contain "HDA_CONFIG=gain,DMIC_CH=2"
	if (${last_index} EQUAL 2)
		list(GET tplg ${last_index} defines)
	endif()

	add_alsatplg2_command("${CMAKE_CURRENT_BINARY_DIR}/../abi.conf" abi_target
	  "${CMAKE_CURRENT_SOURCE_DIR}/../${input}" "${output}"
	  "${CMAKE_CURRENT_SOURCE_DIR}/../" "${defines}")

	add_custom_target(topology2_dev_${output} DEPENDS ${output}.tplg)
	add_dependencies(topology2_dev topology2_dev_${output})
endforeach()
