# SPDX-License-Identifier: BSD-3-Clause

include(${CMAKE_CURRENT_LIST_DIR}/defconfigs.cmake)


           ### configure-time .config ###


if(NOT INIT_CONFIG_found)
# Brand new build directory, search for initial configuration

# Default value when no -DINIT_CONFIG on the command line
set(INIT_CONFIG "initial.config" CACHE STRING "Initial .config file")

# - ".' is the top source directory.
# - "src/arch/${arch}/configs" is for convenience and compatibility with
#   defconfigs.cmake.
# - First found wins.
# - If two archs ever use the same platform_defconfig name then a full
#   path must be used, e.g.: -DINIT_CONFIG=src/arch/myarch/collision_defconfig

set(init_config_search_list ".")
foreach(arch "xtensa" "host")
list(APPEND init_config_search_list "src/arch/${arch}/configs")
endforeach()

find_file(INIT_CONFIG_found
	NAMES ${INIT_CONFIG}
	NO_CMAKE_FIND_ROOT_PATH
	NO_DEFAULT_PATH
	PATHS ${init_config_search_list}
)

else()  # old build directory

if (INIT_CONFIG)
message(WARNING
	"IGNORING '-DINIT_CONFIG=${INIT_CONFIG}!!' "
	"Using up-to-date ${INIT_CONFIG_found} instead."
)
endif()

endif() # new/old build directory


if(NOT INIT_CONFIG_found)
message(FATAL_ERROR
	"Initial configuration missing, no ${INIT_CONFIG} found. "
	"Provide a ${PROJECT_SOURCE_DIR}/initial.config file or specify some "
	"other -DINIT_CONFIG=location relative to '${PROJECT_SOURCE_DIR}/' or "
	"'${PROJECT_SOURCE_DIR}/src/arch/*/configs/'"
)
endif()

# Did someone or something remove our generated/.config?
if(NOT EXISTS ${INIT_CONFIG_found})
message(FATAL_ERROR "The file ${INIT_CONFIG_found} vanished!")
endif()

# Don't confuse this configure-time, .config generation with
# the build-time, autoconfig.h genconfig target below
message(STATUS
	"(Re-)generating ${DOT_CONFIG_PATH}\n"
	"   and ${CONFIG_H_PATH}\n"
	"   from ${INIT_CONFIG_found}"
)
execute_process(
	COMMAND ${CMAKE_COMMAND} -E env
		KCONFIG_CONFIG=${INIT_CONFIG_found}
		srctree=${PROJECT_SOURCE_DIR}
		CC_VERSION_TEXT=${CC_VERSION_TEXT}
		ARCH=${ARCH}
		${PYTHON3} ${PROJECT_SOURCE_DIR}/scripts/kconfig/genconfig.py
		--config-out=${DOT_CONFIG_PATH}
		--header-path ${CONFIG_H_PATH}
		${PROJECT_SOURCE_DIR}/Kconfig
	WORKING_DIRECTORY ${GENERATED_DIRECTORY}
	# Available only from CMake 3.18. Amazingly not the default.
	# COMMAND_ERROR_IS_FATAL ANY
	RESULT_VARIABLE _genret
)
if(${_genret})
message(FATAL_ERROR
  "genconfig.py from ${INIT_CONFIG_found} to ${DOT_CONFIG_PATH} failed")
endif()

if(NOT ${INIT_CONFIG_found} STREQUAL ${DOT_CONFIG_PATH})
# Brand new build directory and config.
message(STATUS
	"Done, future changes to ${INIT_CONFIG_found}\n"
	"   will be IGNORED by this build directory! The primary .config\n"
	"   file is now 'generated/.config' in the build directory."
)
endif()
# Now force CMake to forget about the initial config and to re-use our
# own private ${DOT_CONFIG_PATH} when it decides it must re-run itself.
unset(INIT_CONFIG CACHE)
set(INIT_CONFIG_found ${DOT_CONFIG_PATH} CACHE FILEPATH "active .config" FORCE)


           ###  build-time Kconfig targets ###

add_custom_target(
	menuconfig
	COMMAND ${CMAKE_COMMAND} -E env
		srctree=${PROJECT_SOURCE_DIR}
		CC_VERSION_TEXT=${CC_VERSION_TEXT}
		ARCH=${ARCH}
		${PYTHON3} ${PROJECT_SOURCE_DIR}/scripts/kconfig/menuconfig.py
		${PROJECT_SOURCE_DIR}/Kconfig
	WORKING_DIRECTORY ${GENERATED_DIRECTORY}
	VERBATIM
	USES_TERMINAL
)

add_custom_target(
	overrideconfig
	COMMAND ${CMAKE_COMMAND} -E env
		srctree=${PROJECT_SOURCE_DIR}
		CC_VERSION_TEXT=${CC_VERSION_TEXT}
		ARCH=${ARCH}
		${PYTHON3} ${PROJECT_SOURCE_DIR}/scripts/kconfig/overrideconfig.py
		${PROJECT_SOURCE_DIR}/Kconfig
		${PROJECT_BINARY_DIR}/override.config
	WORKING_DIRECTORY ${GENERATED_DIRECTORY}
	VERBATIM
	USES_TERMINAL
)

file(GLOB_RECURSE KCONFIG_FILES "${SOF_ROOT_SOURCE_DIRECTORY}/Kconfig")


# Don't confuse this build-time, .h target with the
#  configure-time, .config genconfig above.
add_custom_command(
	OUTPUT ${CONFIG_H_PATH}
	COMMAND ${CMAKE_COMMAND} -E env
		srctree=${PROJECT_SOURCE_DIR}
		CC_VERSION_TEXT=${CC_VERSION_TEXT}
		ARCH=${ARCH}
		${PYTHON3} ${PROJECT_SOURCE_DIR}/scripts/kconfig/genconfig.py
		--header-path ${CONFIG_H_PATH}
		${PROJECT_SOURCE_DIR}/Kconfig
	DEPENDS ${DOT_CONFIG_PATH}
	WORKING_DIRECTORY ${GENERATED_DIRECTORY}
	COMMENT "Generating ${CONFIG_H_PATH}"
	VERBATIM
	USES_TERMINAL
)

add_custom_target(genconfig DEPENDS ${CONFIG_H_PATH})

add_custom_target(
	olddefconfig
	COMMAND ${CMAKE_COMMAND} -E env
		srctree=${PROJECT_SOURCE_DIR}
		CC_VERSION_TEXT=${CC_VERSION_TEXT}
		ARCH=${ARCH}
		${PYTHON3} ${PROJECT_SOURCE_DIR}/scripts/kconfig/olddefconfig.py
		${PROJECT_SOURCE_DIR}/Kconfig
	WORKING_DIRECTORY ${GENERATED_DIRECTORY}
	VERBATIM
	USES_TERMINAL
)

add_custom_target(
	alldefconfig
	COMMAND ${CMAKE_COMMAND} -E env
		srctree=${PROJECT_SOURCE_DIR}
		CC_VERSION_TEXT=${CC_VERSION_TEXT}
		ARCH=${ARCH}
		${PYTHON3} ${PROJECT_SOURCE_DIR}/scripts/kconfig/alldefconfig.py
		${PROJECT_SOURCE_DIR}/Kconfig
	WORKING_DIRECTORY ${GENERATED_DIRECTORY}
	VERBATIM
	USES_TERMINAL
)

add_custom_target(
	savedefconfig
	COMMAND ${CMAKE_COMMAND} -E env
		srctree=${PROJECT_SOURCE_DIR}
		CC_VERSION_TEXT=${CC_VERSION_TEXT}
		ARCH=${ARCH}
		${PYTHON3} ${PROJECT_SOURCE_DIR}/scripts/kconfig/savedefconfig.py
		${PROJECT_SOURCE_DIR}/Kconfig
		${PROJECT_BINARY_DIR}/defconfig
	WORKING_DIRECTORY ${GENERATED_DIRECTORY}
	COMMENT "Saving minimal configuration to: ${PROJECT_BINARY_DIR}/defconfig"
	VERBATIM
	USES_TERMINAL
)
