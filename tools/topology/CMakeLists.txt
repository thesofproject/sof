set(SOF_TOPOLOGY_BINARY_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

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
		COMMAND alsatplg \$\${VERBOSE:+-v 1} -c ${ARGV0} -o ${ARGV1}
		USES_TERMINAL
	)
endmacro()

macro(add_alsatplg2_command input_file output_file include_path)
	# command line definitions are optional
	set (defines "")
	set (optional_args ${ARGN})

	# Set defines if there is an optional argument
	list(LENGTH optional_args optional_args_count)
	if (${optional_args_count} GREATER 0)
		list(GET optional_args 0 defines)
	endif()

	if (defines STREQUAL "")
		add_custom_command(
			MAIN_DEPENDENCY ${input_file}
			OUTPUT ${output_file}
			# -p to pre-process Topology2.0 conf file
			COMMAND alsatplg \$\${VERBOSE:+-v 1} -I ${include_path} -p -c ${input_file} -o ${output_file}
			USES_TERMINAL
		)
	else()
		add_custom_command(
			MAIN_DEPENDENCY ${input_file}
			OUTPUT ${output_file}
			# -p to pre-process Topology2.0 conf file
			COMMAND alsatplg \$\${VERBOSE:+-v 1} -I ${include_path} -D ${defines} -p -c ${input_file} -o ${output_file}
			USES_TERMINAL
		)
	endif()
endmacro()


add_custom_target(topologies ALL)
add_dependencies(topologies topologies1)

add_subdirectory(topology1)
add_subdirectory(topology2)
