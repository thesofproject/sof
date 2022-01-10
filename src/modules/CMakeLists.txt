if(CONFIG_BAYTRAIL)
	set(platform_folder baytrail)
elseif(CONFIG_CHERRYTRAIL)
	set(platform_folder baytrail)
elseif(CONFIG_HASWELL)
	set(platform_folder haswell)
elseif(CONFIG_BROADWELL)
	set(platform_folder haswell)
elseif(CONFIG_APOLLOLAKE)
	set(platform_folder apollolake)
elseif(CONFIG_CANNONLAKE)
	set(platform_folder cannonlake)
elseif(CONFIG_SUECREEK)
	set(platform_folder suecreek)
elseif(CONFIG_ICELAKE)
	set(platform_folder icelake)
elseif(CONFIG_TIGERLAKE)
	set(platform_folder tigerlake)
elseif(CONFIG_IMX8)
	set(platform_folder imx8)
elseif(CONFIG_IMX8X)
	set(platform_folder imx8)
elseif(CONFIG_IMX8M)
	set(platform_folder imx8m)
elseif(CONFIG_IMX8ULP)
	set(platform_folder imx8ulp)
elseif(CONFIG_MT8195)
	set(platform_folder mt8195)
endif()

if(CONFIG_CAVS)
	set(family_path intel/cavs)
endif()

function(sof_add_module_ld_script binary_name script_name)

	set(lds_in ${PROJECT_SOURCE_DIR}/src/modules/${binary_name}/${script_name}.in)
	set(lds_out ${PROJECT_BINARY_DIR}/${script_name})

	get_target_property(incdirs sof_options INTERFACE_INCLUDE_DIRECTORIES)

	set(iflags "")
	set(glob_predicates "")
	foreach(d ${incdirs})
		list(APPEND iflags "-I${d}")
		list(APPEND glob_predicates "${d}/*.h")
	endforeach()

	get_target_property(incdirs sof_public_headers INTERFACE_INCLUDE_DIRECTORIES)

	foreach(d ${incdirs})
		list(APPEND iflags "-I${d}")
		list(APPEND glob_predicates "${d}/*.h")
	endforeach()

	file(GLOB lds_headers ${glob_predicates})

	add_custom_command(OUTPUT ${lds_out}
		COMMAND ${CMAKE_C_COMPILER} -E -DLINKER -P ${iflags} -o ${lds_out} -x c ${lds_in}
			-imacros${CONFIG_H_PATH}
		DEPENDS ${lds_in} ${LINK_DEPS} genconfig ${CONFIG_H_PATH} ${lds_headers}
		WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
		COMMENT "Generating linker script: ${lds_out}"
		VERBATIM
		USES_TERMINAL
	)

	add_custom_target("ld_script_${script_name}" DEPENDS ${lds_out})
	add_dependencies(${binary_name} "ld_script_${script_name}")
	set_target_properties(${binary_name} PROPERTIES LINK_DEPENDS ${lds_in})
endfunction()


add_subdirectory(template)