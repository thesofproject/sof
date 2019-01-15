# Looks for defconfig files in arch directory
set(DEFCONFIGS_DIRECTORY "${PROJECT_SOURCE_DIR}/src/arch/${ARCH}/configs/*_defconfig")
file(GLOB DEFCONFIG_PATHS ${DEFCONFIGS_DIRECTORY})

# Adds dependency on defconfigs directory
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${DEFCONFIGS_DIRECTORY})

# Adds target for every defconfig, so you we can use it like make *_defconfig
foreach(defconfig_path ${DEFCONFIG_PATHS})
	get_filename_component(defconfig_name ${defconfig_path} NAME)
	add_custom_target(
		${defconfig_name}
		COMMAND ${CMAKE_COMMAND} -E copy
			${defconfig_path}
			${DOT_CONFIG_PATH}
		COMMAND ${CMAKE_COMMAND} -E env
			srctree=${PROJECT_SOURCE_DIR}
			CC_VERSION_TEXT=${CC_VERSION_TEXT}
			ARCH=${ARCH}
			${PYTHON3} ${PROJECT_SOURCE_DIR}/scripts/kconfig/olddefconfig.py
			${PROJECT_SOURCE_DIR}/Kconfig
		WORKING_DIRECTORY ${GENERATED_DIRECTORY}
		COMMENT "Applying olddefconfig with ${defconfig_name}"
		VERBATIM
		USES_TERMINAL
	)
endforeach()
