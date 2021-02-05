# SPDX-License-Identifier: BSD-3-Clause

# Reads configs from kconfig file and set them as cmake variables.
# Each config is in format CONFIG_<NAME>=<VALUE>.
# Configs are added to parent scope with CONFIG_ prefix (as written in file).
function(read_kconfig_config config_file)
	file(
		STRINGS
		${config_file}
		configs_list
		REGEX "^CONFIG_"
		ENCODING "UTF-8"
	)

	foreach(config ${configs_list})
		string(REGEX MATCH "^([^=]+)=(.*)$" ignored ${config})
		set(config_name ${CMAKE_MATCH_1})
		set(config_value ${CMAKE_MATCH_2})

		if("${config_value}" MATCHES "^\"(.*)\"$")
			set(config_value ${CMAKE_MATCH_1})
		endif()

		set("${config_name}" "${config_value}" PARENT_SCOPE)
	endforeach()
endfunction()

# create optimization flags based on cmake variables set from Kconfig
function(get_optimization_flag OUT_VAR)
	if(CONFIG_OPTIMIZE_FOR_PERFORMANCE)
		set(${OUT_VAR} "O2" PARENT_SCOPE)
	elseif(CONFIG_OPTIMIZE_FOR_SIZE)
		set(${OUT_VAR} "Os" PARENT_SCOPE)
	elseif(CONFIG_OPTIMIZE_FOR_DEBUG)
		set(${OUT_VAR} "Og" PARENT_SCOPE)
	elseif(CONFIG_OPTIMIZE_FOR_NONE)
		set(${OUT_VAR} "O0" PARENT_SCOPE)
	else()
		message(FATAL_ERROR "no CONFIG_OPTIMIZE_ found")
	endif()
endfunction()

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
	# -imacros${CONFIG_H_PATH} escapes regular .h dep scanning
	#	add_dependencies(${target} genconfig) # has no effect?
		set_source_files_properties(${path}
			PROPERTIES
			OBJECT_DEPENDS ${CONFIG_H_PATH}
		)
	endforeach()
endfunction()

# Declares new static lib with given name and path that will be linked
# to sof binary.
function(sof_add_static_library lib_name lib_path)
	# we need libs to be visible in the root CMakeLists, so use GLOBAL
	add_library(${lib_name} STATIC IMPORTED GLOBAL)

	if(IS_ABSOLUTE ${lib_path})
		set(lib_abs_path ${lib_path})
	else()
		set(lib_abs_path ${CMAKE_CURRENT_SOURCE_DIR}/${lib_path})
	endif()

	set_target_properties(${lib_name} PROPERTIES IMPORTED_LOCATION ${lib_abs_path})
	target_link_libraries(sof_static_libraries INTERFACE ${lib_name})
endfunction()

# Appends literal with path of the source file relative to the project root
# It is useful if sources in given target need deterministic relative path
# to the actually compiled file.
# __FILE is not always suitable as C standard states that __FILE__ expands to
# input file name, that usually is absolute path what will cause f.e. .rodata
# size to be dependent on where project is physically located on the disk.
function(sof_append_relative_path_definitions target)
	get_target_property(sources ${target} SOURCES)
	foreach(src ${sources})
		get_filename_component(ABS_PATH ${src} ABSOLUTE)
		file(RELATIVE_PATH rel ${PROJECT_SOURCE_DIR} ${ABS_PATH})
		set_property(
			SOURCE ${src}
			APPEND
			PROPERTY COMPILE_DEFINITIONS
			RELATIVE_FILE="${rel}")
	endforeach()
endfunction()
