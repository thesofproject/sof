
if(NOT DEFINED SIGNING_KEY)
	message(FATAL_ERROR
		" Please define SIGNING_KEY: path to signing key for rimage.\n"
		" E.g. using cmake -DSIGNING_KEY=/path/to/key.pem command line parameter."
	)
endif()

# This Loadable Modules Dev Kit root dir
set(LMDK_BASE ${CMAKE_CURRENT_LIST_DIR}/..)
cmake_path(ABSOLUTE_PATH LMDK_BASE NORMALIZE)

# thesofproject root dir
set(SOF_BASE ${LMDK_BASE}/..)
cmake_path(ABSOLUTE_PATH SOF_BASE NORMALIZE)

set(RIMAGE_INCLUDE_DIR ${SOF_BASE}/tools/rimage/src/include)
cmake_path(ABSOLUTE_PATH RIMAGE_INCLUDE_DIR NORMALIZE)

# Adds sources to target like target_sources, but assumes that
# paths are relative to subdirectory.
# Works like:
# 	Cmake >= 3.13:
#		target_sources(<target> PRIVATE <sources>)
# 	Cmake < 3.13:
#		target_sources(<target> PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/<sources>)
function(add_local_sources target)
	foreach(arg ${ARGN})
		if(IS_ABSOLUTE ${arg})
			set(path ${arg})
		else()
			set(path ${CMAKE_CURRENT_SOURCE_DIR}/${arg})
		endif()

		target_sources(${target} PRIVATE ${path})
	endforeach()
endfunction()

# Currently loadable modules do not support the Zephyr build system
macro(is_zephyr ret)
	set(${ret} FALSE)
endmacro()
