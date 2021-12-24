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


add_custom_target(topologies ALL)
add_dependencies(topologies topologies1)

add_subdirectory(topology1)
