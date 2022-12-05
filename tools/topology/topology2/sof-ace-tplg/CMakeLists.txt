# SPDX-License-Identifier: BSD-3-Clause

# Array of "input-file-name;output-file-name;comma separated pre-processor variables"
set(TPLGS
# HDMI only topology with passthrough pipelines
"sof-hda-generic\;sof-hda-generic-idisp\;USE_CHAIN_DMA=true,DEEPBUFFER_FW_DMA_MS=100"
# HDA topology with mixer-based pipelines for HDA and passthrough pipelines for HDMI
"sof-hda-generic\;sof-hda-generic\;HDA_CONFIG=mix,USE_CHAIN_DMA=true,DEEPBUFFER_FW_DMA_MS=100"
# If the alsatplg plugins for NHLT are not available, the NHLT blobs will not be added to the
# topologies below.
"sof-hda-generic\;sof-hda-generic-4ch\;PLATFORM=mtl,\
HDA_CONFIG=mix,NUM_DMICS=4,PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,PREPROCESS_PLUGINS=nhlt,\
NHLT_BIN=nhlt-sof-hda-generic-4ch.bin,DEEPBUFFER_FW_DMA_MS=100"
"sof-hda-generic\;sof-hda-generic-2ch\;PLATFORM=mtl,\
HDA_CONFIG=mix,NUM_DMICS=2,PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-hda-generic-2ch.bin,\
DEEPBUFFER_FW_DMA_MS=100"

# SDW topology with passthrough pipelines
"cavs-sdw\;mtl-sdw"

# SDW + HDMI topology with passthrough pipelines
"cavs-sdw\;mtl-sdw-hdmi\;NUM_HDMIS=4"

# SDW + DMIC topology with passthrough pipelines
"cavs-sdw\;sof-mtl-rt711-4ch\;NUM_DMICS=4,DMIC0_ID=2,DMIC1_ID=3,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-mtl-rt711-4ch.bin,DEEPBUFFER_FW_DMA_MS=100"

# SSP topology for MTL
"cavs-nocodec\;sof-mtl-nocodec\;PLATFORM=mtl,NUM_DMICS=2,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-mtl-nocodec.bin,DEEPBUFFER_FW_DMA_MS=100"

"cavs-rt5682\;sof-mtl-max98357a-rt5682\;PLATFORM=mtl,NUM_DMICS=4,PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,NUM_HDMIS=4,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-mtl-max98357a-rt5682.bin,DEEPBUFFER_FW_DMA_MS=10"
)

add_custom_target(topology2_ace)

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

	add_custom_target(topology2_ace_${output} DEPENDS ${output}.tplg)
	add_dependencies(topology2_ace topology2_ace_${output})
endforeach()
