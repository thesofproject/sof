# SPDX-License-Identifier: BSD-3-Clause

# Looks for defconfig files in arch directories where kconfig.cmake looks too.
set(DEFCONFIGS_DIRECTORY "${PROJECT_SOURCE_DIR}/src/arch/${ARCH}/configs")
file(GLOB DEFCONFIG_PATHS "${DEFCONFIGS_DIRECTORY}/*_defconfig")

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

set(OVERRIDE_DEFCONFIGS_DIRECTORY "${DEFCONFIGS_DIRECTORY}/override")
file(GLOB OVERRIDE_DEFCONFIGS_PATHS "${OVERRIDE_DEFCONFIGS_DIRECTORY}/*.config")

foreach(config_path ${OVERRIDE_DEFCONFIGS_PATHS})
	get_filename_component(config_name ${config_path} NAME_WE)
	add_custom_target(
		"${config_name}_overridedefconfig"
		COMMAND ${CMAKE_COMMAND} -E copy
			${config_path}
			${PROJECT_BINARY_DIR}/override.config
		COMMAND ${CMAKE_COMMAND} -E env
			srctree=${PROJECT_SOURCE_DIR}
			CC_VERSION_TEXT=${CC_VERSION_TEXT}
			ARCH=${ARCH}
			${PYTHON3} ${PROJECT_SOURCE_DIR}/scripts/kconfig/overrideconfig.py
			${PROJECT_SOURCE_DIR}/Kconfig
			${PROJECT_BINARY_DIR}/override.config
		WORKING_DIRECTORY ${GENERATED_DIRECTORY}
		COMMENT "Applying overrideconfig with ${config_name}"
		VERBATIM
		USES_TERMINAL
	)
endforeach()
