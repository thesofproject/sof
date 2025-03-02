set(SOF_TOPOLOGY_BINARY_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
set(SOF_ALSA_TOOLS_DIR "${SOF_ROOT_SOURCE_DIRECTORY}/../tools/bin")
set(ALSATPLG_CMD "${SOF_ALSA_TOOLS_DIR}/alsatplg")

function(alsatplg_version  OUT_STATUS  OUT_VERSION)
	execute_process(COMMAND ${ALSATPLG_CMD} --version
	  RESULT_VARIABLE status
	  OUTPUT_VARIABLE stdout
	  OUTPUT_STRIP_TRAILING_WHITESPACE)
	message(DEBUG "${ALSATPLG_CMD} --version: status=${status}, output=${stdout}")

	set(${OUT_STATUS} "${status}" PARENT_SCOPE)

        # Some error messages have already been printed on stderr
	if(NOT status EQUAL 0)
	  message(WARNING "${ALSATPLG_CMD} --version returned status: ${status},
${stdout}")
	  return()
	endif()

	string(REPLACE "\n" ";" ALSA_VERSION_LIST ${stdout})
	list(GET ALSA_VERSION_LIST 0 ALSATPLG_VERSION)
	string(REGEX MATCH "[0-9]\.[0-9]\.*[0-9]*" ALSATPLG_VERSION_NUMBER ${ALSATPLG_VERSION})

	set(${OUT_VERSION} "${ALSATPLG_VERSION_NUMBER}" PARENT_SCOPE)
endfunction()


# Being written in C, `alsatplg` silently ignores some invalid inputs
# and produces an corrupt .tplg file instead of returning an error code
# that fails the build.  For instance, alsatplg versions < 1.2.5 silently
# corrupt `codec_consumer` and turn it into `codec_master` instead.
# Longer story in #5192.
alsatplg_version(STATUS ALSATPLG_VERSION_NUMBER)
if(NOT STATUS EQUAL 0)
	message(WARNING "${ALSATPLG_CMD} failed: ${STATUS}; all topologies skipped")
	return()
else()
	if(${ALSATPLG_VERSION_NUMBER} VERSION_LESS "1.2.5")
		message(WARNING "All topologies skipped: minimum alsatplg version 1.2.5,\
 found ${ALSATPLG_VERSION_NUMBER}.")
		return()
	endif()
	# success
endif()


# This use of VERBOSE relies on original CMake behavior.
# From the add_custom_command() manual:
#
#    Use of VERBATIM is recommended as it enables correct behavior.
#    When VERBATIM is not given the behavior is platform specific because
#    there is no protection of tool-specific special characters.
#
# This is fine because:
# - We don't expect alsatplg to work on Windows any time soon.
# - CMake is too afraid to remove the original, no-VERBATIM behavior:
#      https://gitlab.kitware.com/cmake/cmake/issues/18849
#
# Also note that in alsa-utils commit v1.2.2~15-gcbabe7a3f0cc, alsatplg
# (accidentally?) started ignoring the verbosity level, now it's just
# verbose or not.
macro(add_alsatplg_command)
	add_custom_command(
		MAIN_DEPENDENCY ${ARGV0}
		OUTPUT ${ARGV1}
		# Warning: before alsa-utils fix 8e71fba810b87c,
		# permissions are hardcoded and only the user can read
		# the -o(utput) file.
		# See bug https://github.com/alsa-project/alsa-utils/issues/126
		COMMAND ${ALSATPLG_CMD} \$\${VERBOSE:+-v 1} -c ${ARGV0} -o ${ARGV1}
		USES_TERMINAL
	)
endmacro()

macro(add_alsatplg2_command conf_header conf_target input_name output_name include_path)
	# command line definitions are optional
	# Set defines if there is an optional argument
	if (${ARGC} GREATER 5)
		set (defines ${ARGV5})
	else()
		set (defines "")
	endif()

	add_custom_command(
		MAIN_DEPENDENCY ${input_name}.conf
		# The conf_header file is the real dependency that
		# triggers a rebuild. conf_target is the target that
		# avoids a race to rebuild conf_header. See the CMake FAQ
		# or (better) Sam Thursfield's blog about custom
		# targets.
		DEPENDS ${conf_header} ${conf_target}
		OUTPUT ${output_name}.tplg

		COMMAND cat ${conf_header} ${input_name}.conf > ${output_name}.conf

		# -p to pre-process Topology2.0 conf file
		COMMAND ALSA_CONFIG_DIR=${CMAKE_SOURCE_DIR}/topology/topology2 ${ALSATPLG_CMD} \$\${VERBOSE:+-v 1}
		        -I ${include_path} -D "'${defines}'" -p -c ${output_name}.conf -o ${output_name}.tplg
		USES_TERMINAL
		)
endmacro()


add_custom_target(topologies ALL)
add_dependencies(topologies topologies1)

add_subdirectory(topology1)
add_subdirectory(topology2)
