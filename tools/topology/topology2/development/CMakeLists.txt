# SPDX-License-Identifier: BSD-3-Clause

# Array of "input-file-name;output-file-name;comma separated pre-processor variables"
set(TPLGS
# CAVS SSP topology for MTL for electrical validation
# Default MCLK = 24576000, BCLK = 3072000, FSYNC = 48000,
# TDM_SLOTS = 2, TDM_WIDTH = 32
# Verify with
# MCLK = 38400000, BCLK = 4800000, FSYNC = 48000,
# TDM_SLOTS = 4, TDM_WIDTH = 25
"cavs-nocodec-val\;sof-mtl-nocodec-m384-b48-f48\;PLATFORM=mtl,NUM_DMICS=2,\
MCLK=38400000,BCLK=4800000,TDM_SLOTS=4,TDM_WIDTH=25,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-ace-mtl-nocodec.bin"
# Verify with
# MCLK = 19200000, BCLK = 6144000, FSYNC = 48000,
# TDM_SLOTS = 4, TDM_WIDTH = 32
"cavs-nocodec-val\;sof-mtl-nocodec-m192-b6144-f48\;PLATFORM=mtl,NUM_DMICS=2,\
MCLK=19200000,BCLK=6144000,TDM_SLOTS=4,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-ace-mtl-nocodec.bin"
# Verify with
# MCLK = 19200000, BCLK = 12280000, FSYNC = 96000,
# TDM_SLOTS = 4, TDM_WIDTH = 32
"cavs-nocodec-val\;sof-mtl-nocodec-m192-b1228-f96\;PLATFORM=mtl,NUM_DMICS=2,\
MCLK=19200000,BCLK=12280000,TDM_SLOTS=4,FSYNC=96000,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-ace-mtl-nocodec.bin"
# Verify with
# MCLK = 38400000, BCLK = 4800000, FSYNC = 96000,
# TDM_SLOTS = 2, TDM_WIDTH = 25
"cavs-nocodec-val\;sof-mtl-nocodec-m384-b48-f96\;PLATFORM=mtl,NUM_DMICS=2,\
MCLK=38400000,BCLK=4800000,TDM_WIDTH=25,FSYNC=96000,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-ace-mtl-nocodec.bin"
# Verify with
# MCLK = 19200000, BCLK = 6144000, FSYNC = 96000,
# TDM_SLOTS = 4, TDM_WIDTH = 32
"cavs-nocodec-val\;sof-mtl-nocodec-m192-b6144-f96\;PLATFORM=mtl,NUM_DMICS=2,\
MCLK=19200000,BCLK=6144000,TDM_SLOTS=4,FSYNC=96000,\
PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt-ace-mtl-nocodec.bin"
)

# This will override the topology1 binaries with topology2 binaries
add_custom_target(topology2_development)
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
	configure_file(${CMAKE_CURRENT_BINARY_DIR}/../cavs/abi.conf ${CMAKE_CURRENT_BINARY_DIR}/${output}.conf)
	file(READ ${CMAKE_CURRENT_SOURCE_DIR}/${input}.conf CONTENTS)
	file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/${output}.conf "${CONTENTS}")

	add_alsatplg2_command("${output}.conf" "${output}.tplg" "${CMAKE_CURRENT_SOURCE_DIR}/../cavs" "${defines}")
	add_custom_target(topology2_${output} DEPENDS ${output}.tplg)
	add_dependencies(topology2_cavs topology2_${output})
endforeach()
