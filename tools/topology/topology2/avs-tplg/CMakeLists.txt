# SPDX-License-Identifier: BSD-3-Clause

# Array of "input-file-name;output-file-name;comma separated pre-processor variables"
set(TPLGS
# CAVS HDMI only topology with passthrough pipelines
"cavs-passthrough-hdmi\;sof-hda-generic-idisp\;USE_CHAIN_DMA=true"
# CAVS HDA topology with mixer-based pipelines for HDA and passthrough pipelines for HDMI
"cavs-passthrough-hdmi\;sof-hda-generic\;HDA_CONFIG=mix,USE_CHAIN_DMA=true"
# If the alsatplg plugins for NHLT are not available, the NHLT blobs will not be added to the
# topologies below.
"cavs-passthrough-hdmi\;sof-hda-generic-2ch\;\
HDA_CONFIG=mix,NUM_DMICS=2,PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-hda-generic-2ch.bin,USE_CHAIN_DMA=true"
"cavs-passthrough-hdmi\;sof-hda-generic-4ch\;\
HDA_CONFIG=mix,NUM_DMICS=4,PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-hda-generic-4ch.bin,USE_CHAIN_DMA=true"
# CAVS SDW topology with passthrough pipelines
"cavs-sdw\;cavs-sdw"

# CAVS SDW with SRC gain and mixer support
"cavs-sdw-src-gain-mixin\;cavs-sdw-src-gain-mixin"

# SDW + HDMI topology with passthrough pipelines
"cavs-sdw\;cavs-sdw-hdmi\;NUM_HDMIS=4"

# CAVS SSP topology for TGL
"cavs-nocodec\;sof-tgl-nocodec\;NUM_DMICS=4,PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-sof-tgl-nocodec.bin"
)

add_custom_target(topology2_cavs)

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

	add_custom_target(topology2_avs_${output} DEPENDS ${output}.tplg)
	add_dependencies(topology2_cavs topology2_avs_${output})
endforeach()
