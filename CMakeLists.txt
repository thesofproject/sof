cmake_minimum_required(VERSION 3.10)

if("${CMAKE_CURRENT_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_BINARY_DIR}")
	message(FATAL_ERROR
		" In-source builds are not supported.\n"
		" Please remove CMakeCache.txt and the CMakeFiles directory.\n"
		" Then specify a build directory. Example: cmake -Bbuild ..."
	)
endif()

if(NOT (${CMAKE_VERSION} VERSION_LESS "3.13.0"))
	# CMake 3.13+ has less restrictive rules for target_link_libraries()
	# Until we make 3.13 as required minimum version we want to
	# use old behaviour for compatibility
	cmake_policy(SET CMP0079 OLD)
endif()

# TODO: take `host` out of fw build
option(BUILD_HOST "Build host program" OFF)
option(BUILD_UNIT_TESTS "Build unit tests" OFF)

if(BUILD_HOST)
	set(ARCH host)
else()
	# firmware build supports only xtensa arch for now
	set(ARCH xtensa)
endif()

# let user override built-in toolchains
if(DEFINED CMAKE_TOOLCHAIN_FILE)
	set(TOOLCHAIN "${CMAKE_TOOLCHAIN_FILE}")
else()
	include(scripts/cmake/${ARCH}-toolchain.cmake OPTIONAL)
endif()

include(scripts/cmake/misc.cmake)

project(SOF C ASM)

# intended to be used where PROJECT_* variables are not set
# for example in standalone scripts like version.cmake
set(SOF_ROOT_SOURCE_DIRECTORY "${PROJECT_SOURCE_DIR}")
set(SOF_ROOT_BINARY_DIRECTORY "${PROJECT_BINARY_DIR}")

# check git hooks
include(scripts/cmake/git-hooks.cmake)

# most of other options are set on per-arch and per-target basis
set(CMAKE_ASM_FLAGS -DASSEMBLY)

# interface library that is used only as container for sof binary options
# other targets can use it to build with the same options
add_library(sof_options INTERFACE)

target_include_directories(sof_options INTERFACE ${PROJECT_SOURCE_DIR}/src/include)

if(BUILD_HOST)
	add_subdirectory(src)
	# rest of this file is not needed for host build
	return()
endif()

# get compiler name and version
execute_process(
	COMMAND ${CMAKE_C_COMPILER} --version
	OUTPUT_VARIABLE cc_version_output
	OUTPUT_STRIP_TRAILING_WHITESPACE
	ERROR_QUIET
)

string(REGEX MATCH "^[^\r\n]+" CC_VERSION_TEXT "${cc_version_output}")
if(NOT CC_VERSION_TEXT)
	message(WARNING "Couldn't get compiler version")
	set(CC_VERSION_TEXT 0)
endif()

set(GENERATED_DIRECTORY ${PROJECT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED_DIRECTORY}/include)

set(DOT_CONFIG_PATH ${GENERATED_DIRECTORY}/.config)
set(CONFIG_H_PATH ${GENERATED_DIRECTORY}/include/config.h)
set(VERSION_H_PATH ${GENERATED_DIRECTORY}/include/version.h)

include(scripts/cmake/version.cmake)

include(scripts/cmake/dist.cmake)

# cmake itself cannot depend on files that don't exist
# so to make it regenerate when .config file is created,
# we make it depend on containing directory
# NOTE: some filesystems may be unable to watch directory change,
#       in that case cmake may need to be regenerated from CLI
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
	${GENERATED_DIRECTORY} ${DOT_CONFIG_PATH})

if(EXISTS ${DOT_CONFIG_PATH})
	read_kconfig_config(${DOT_CONFIG_PATH})
endif()

if(${CMAKE_VERSION} VERSION_LESS "3.12.0")
	# FindPythonInterp is bugged and may sometimes be unable to find
	# Python 3 when both Python 2 & 3 are in PATH,
	# so it's always better to use CMake 3.12+
	find_package(PythonInterp 3.0)
	set(PYTHON3 "${PYTHON_EXECUTABLE}")
else()
	find_package(Python3 COMPONENTS Interpreter)
	set(PYTHON3 "${Python3_EXECUTABLE}")
endif()

include(scripts/cmake/kconfig.cmake)

add_dependencies(sof_options genconfig check_version_h)
target_include_directories(sof_options INTERFACE ${GENERATED_DIRECTORY}/include)

if(BUILD_UNIT_TESTS)
	enable_testing()
	add_subdirectory(src/arch/${ARCH})
	add_subdirectory(test)
	# rest of this file is not needed for unit tests
	return()
endif()

add_library(sof_ld_flags INTERFACE)
add_library(sof_ld_scripts INTERFACE)

# declare target with no sources to let cmake know about it
add_executable(sof "")
target_link_libraries(sof sof_options)
target_link_libraries(sof sof_ld_scripts)
target_link_libraries(sof sof_ld_flags)

sof_add_build_counter_rule()

add_subdirectory(src)

target_link_libraries(sof sof_lib)
