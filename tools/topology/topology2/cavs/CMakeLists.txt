# SPDX-License-Identifier: BSD-3-Clause

# Array of "input-file-name;output-file-name;comma separated pre-processor variables"
set(TPLGS
# CAVS HDMI only topology with passthrough pipelines
"cavs-passthrough-hdmi\;cavs-passthrough-hdmi"
# CAVS HDA topology with mixer-based pipelines for HDA and passthrough pipelines for HDMI
"cavs-passthrough-hdmi\;cavs-mixin-mixout-hda\;HDA_CONFIG=mix"
# If the alsatplg plugins for NHLT are not available, the NHLT blobs will not be added to the
# topologies below.
# CNL: CAVS HDA topology with mixer-based pipelines for HDA and passthrough pipelines for HDMI
"cavs-passthrough-hdmi\;cavs-mixin-mixout-hda-2ch-cnl\;\
HDA_CONFIG=mix,NUM_DMICS=2,PLATFORM=cnl,PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt.bin"
"cavs-passthrough-hdmi\;cavs-mixin-mixout-hda-4ch-cnl\;\
HDA_CONFIG=mix,NUM_DMICS=4,PLATFORM=cnl,PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt.bin"
# TGL: CAVS HDA topology with mixer-based pipelines for HDA and passthrough pipelines for HDMI
"cavs-passthrough-hdmi\;cavs-mixin-mixout-hda-2ch-tgl\;\
HDA_CONFIG=mix,NUM_DMICS=2,PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt.bin"
"cavs-passthrough-hdmi\;cavs-mixin-mixout-hda-4ch-tgl\;\
HDA_CONFIG=mix,NUM_DMICS=4,PLATFORM=cnl,PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt.bin"
# MTL: CAVS HDA topology with mixer-based pipelines for HDA and passthrough pipelines for HDMI
"cavs-passthrough-hdmi\;cavs-mixin-mixout-hda-4ch-mtl\;PLATFORM=mtl,\
HDA_CONFIG=mix,NUM_DMICS=4,PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-mtl-hda-mix.bin"

# CAVS SDW topology with passthrough pipelines
"cavs-sdw\;cavs-sdw"

# CAVS SDW with SRC gain and mixer support
"cavs-sdw-src-gain-mixin\;cavs-sdw-src-gain-mixin"

# SDW + HDMI topology with passthrough pipelines
"cavs-sdw\;cavs-sdw-hdmi\;NUM_HDMIS=4"

# SDW + DMIC topology with passthrough pipelines
"cavs-sdw\;cavs-mtl-sdw-dmic\;NUM_DMICS=4,DMIC0_ID=2,DMIC1_ID=3,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-mtl-sdw-dmic.bin"

# CAVS SSP topology for TGL
"cavs-nocodec\;cavs-tgl-nocodec\;NUM_DMICS=4,PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-cavs-tgl-nocodec.bin"
# SSP topology for MTL
"cavs-nocodec\;sof-mtl-nocodec\;PLATFORM=mtl,NUM_DMICS=2,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-ace-mtl-nocodec.bin"
)

# generate ABI for IPC4
execute_process(
	COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../get_abi.sh ${SOF_ROOT_SOURCE_DIRECTORY} "ipc4"
	OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/abi.conf
)

add_custom_target(topology2_cavs)
add_dependencies(topologies2 topology2_cavs)

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

	# copy ABI and input conf file contents
	#
	# BUG: move this from CMake configuration time to build time to
	# fix incremental builds
	configure_file(${CMAKE_CURRENT_BINARY_DIR}/abi.conf ${CMAKE_CURRENT_BINARY_DIR}/${output}.conf)
	file(READ ${CMAKE_CURRENT_SOURCE_DIR}/${input}.conf CONTENTS)
	file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/${output}.conf "${CONTENTS}")

	add_alsatplg2_command("${output}.conf" "${output}.tplg" "${CMAKE_CURRENT_SOURCE_DIR}" "${defines}")
	add_custom_target(topology2_${output} DEPENDS ${output}.tplg)
	add_dependencies(topology2_cavs topology2_${output})
endforeach()
