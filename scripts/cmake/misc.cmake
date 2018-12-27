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
