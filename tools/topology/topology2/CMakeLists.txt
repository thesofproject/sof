# SPDX-License-Identifier: BSD-3-Clause

# Array of "input-file-name;output-file-name;machine quirks"
# machine quirks are defined as follows:
set(TPLGS
# TGL-U topology with 4ch DMIC
 	"sof-tgl-rt711-rt1308\;sof-tgl-rt711-rt1308-4ch\;DYNAMIC_PIPELINE 0\;iDisp1_ID 5\;iDisp2_ID 6\;iDisp3_ID 7\;iDisp4_ID 8\;\
DMIC_INCLUDE include/platform/intel/dmic-generic.conf\;DMIC0_PIPELINE_ID 4\;DMIC1_PIPELINE_ID 5\;DMIC_PCM_ID 10\;\
DMIC16KHz_PCM_ID 11\;DMIC0_EQIIR_COEF eqiir_highpass_40hz_20db_48khz\;DMIC1_EQIIR_COEF eqiir_highpass_40hz_20db_16khz\;\
DMIC0_ID 3\;DMIC1_ID 4\;CHANNELS 4"

# TGL-H RVP topology with 4ch DMIC
	"sof-tgl-rt711\;sof-tgl-rt711-4ch\;DYNAMIC_PIPELINE 0\;iDisp1_ID 4\;iDisp2_ID 5\;iDisp3_ID 6\;iDisp4_ID 7\;\
DMIC_INCLUDE include/platform/intel/dmic-generic.conf\;DMIC0_PIPELINE_ID 4\;DMIC1_PIPELINE_ID 5\;DMIC_PCM_ID 10\;\
DMIC16KHz_PCM_ID 11\;DMIC0_EQIIR_COEF eqiir_highpass_40hz_20db_48khz\;DMIC1_EQIIR_COEF eqiir_highpass_40hz_20db_16khz\;\
DMIC0_ID 2\;DMIC1_ID 3\;CHANNELS 4"

# ADL-S topology with 4ch DMIC AND BLUETOOTH
	"sof-tgl-rt711\;sof-adl-rt711-4ch\;DYNAMIC_PIPELINE 0\;iDisp1_ID 4\;iDisp2_ID 5\;iDisp3_ID 6\;iDisp4_ID 7\;\
DMIC_INCLUDE include/platform/intel/dmic-generic.conf\;DMIC0_PIPELINE_ID 4\;DMIC1_PIPELINE_ID 5\;DMIC_PCM_ID 10\;\
DMIC16KHz_PCM_ID 11\;DMIC0_EQIIR_COEF eqiir_highpass_40hz_20db_48khz\;DMIC1_EQIIR_COEF eqiir_highpass_40hz_20db_16khz\;\
DMIC0_ID 2\;DMIC1_ID 3\;CHANNELS 4\;SSP_INCLUDE include/platform/intel/bt-generic.conf\;SSP2_ID 8\;\
SSP2_PLAYBACK_PIPELINE_ID 13\;SSP2_CAPTURE_PIPELINE_ID 14\;SSP2_PCM_ID 14"
)

add_custom_target(topologies2 ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory . ${SOF_TOPOLOGY_BINARY_DIRECTORY}
)

add_dependencies(topologies2 topologies1)

# generate ABI object. Only need to do this once for all topologies
execute_process(
	COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/get_abi.sh ${SOF_ROOT_SOURCE_DIRECTORY}
	OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/abi.conf
)

foreach(tplg ${TPLGS})
	set(machine_quirks "")
	list(LENGTH tplg length)
	list(GET tplg 0 input)
	list(GET tplg 1 output)

	# define variables for configuring input conf files with the appropriate values for machine quirks
	math(EXPR last_index "${length}-1")
	foreach(i RANGE 2 ${last_index})
		list(GET tplg ${i} quirk)
		string(REPLACE " " ";" quirk_pair ${quirk})
		list(GET quirk_pair 0 quirk_name)
		list(GET quirk_pair 1 quirk_value)
		set(${quirk_name} ${quirk_value})
	endforeach(i)

	# copy ABI and input conf file contents
	configure_file(${CMAKE_CURRENT_BINARY_DIR}/abi.conf ${CMAKE_CURRENT_BINARY_DIR}/${output}_abi.conf)
	file(READ ${CMAKE_CURRENT_SOURCE_DIR}/${input}.conf CONTENTS)
	file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/${output}_abi.conf "${CONTENTS}")

	# copy DMIC include file if needed
	if(DEFINED DMIC_INCLUDE)
		file(READ ${CMAKE_CURRENT_SOURCE_DIR}/${DMIC_INCLUDE} CONTENTS)
		file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/${output}_abi.conf "${CONTENTS}")

		# set PDM config based on channels
		if(${CHANNELS} EQUAL 1 OR ${CHANNELS} EQUAL 2)
			set(PDM_CONFIG "num_pdm_active 1\n\t\tObject.Base.pdm_config.0{}")
		elseif(${CHANNELS} EQUAL 3 OR ${CHANNELS} EQUAL 4)
			set(PDM_CONFIG "num_pdm_active 2\n\t\tObject.Base.pdm_config.0{}\n\t\tObject.Base.pdm_config.1{}")
		endif(${CHANNELS} EQUAL 1 OR ${CHANNELS} EQUAL 2)
	endif(DEFINED DMIC_INCLUDE)

	# copy SSP include file if needed
	if(DEFINED SSP_INCLUDE)
		file(READ ${CMAKE_CURRENT_SOURCE_DIR}/${SSP_INCLUDE} CONTENTS)
		file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/${output}_abi.conf "${CONTENTS}")
	endif(DEFINED SSP_INCLUDE)

	# configure input file
	configure_file(${CMAKE_CURRENT_BINARY_DIR}/${output}_abi.conf ${CMAKE_CURRENT_BINARY_DIR}/${output}.conf @ONLY)

# Note: this does NOT use VERBATIM, see explanation in ../topology/CMakeLists.txt
	add_custom_command(
		OUTPUT ${output}.tplg
		# -p to process Topology2.0 conf file
		COMMAND alsatplg \$\${VERBOSE:+-v 1} -p -c ${output}.conf -o ${output}.tplg
		DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${output}.conf
		USES_TERMINAL
	)

	add_custom_target(topology2_${output} DEPENDS ${output}.tplg)
	add_dependencies(topologies2 topology2_${output})
endforeach()
