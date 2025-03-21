# When west is installed, Zephyr's CMake invokes west to list and try to
# compile every Zephyr module that can be found. See
# sof/zephyr/module.yml and
# https://docs.zephyrproject.org/latest/develop/modules.html
if(CONFIG_SOF)

if(CONFIG_LIBRARY)
	set(PLATFORM "library")
	set(ARCH host)
	set(PLATFORM_HEADERS "posix")
	zephyr_include_directories(../src/platform/library/include)
elseif(CONFIG_ZEPHYR_POSIX)
	set(ARCH host)
	set(PLATFORM "posix")
	set(PLATFORM_HEADERS "posix")
else()
	# firmware build supports only xtensa arch for now
	set(ARCH xtensa)
	set(PLATFORM_HEADERS "xtos")
endif()

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

define_property(GLOBAL PROPERTY SOF_LLEXT_LAST_TARGET
  BRIEF_DOCS "Last LLEXT target"
  FULL_DOCS "\
Building LLEXT targets must be serialized. This property contains the \
previously added LLEXT module for the establishment of a dependency chain."
)

# Build an LLEXT module. Provice a module name, a list of sources and an address
# of the .text section as arguments.
function(sof_llext_build module)
	set(multi_args SOURCES)
	set(single_args LIB)
	cmake_parse_arguments(PARSE_ARGV 1 SOF_LLEXT "${options}" "${single_args}" "${multi_args}")

	cmake_path(SET SOF_BASE NORMALIZE ${APPLICATION_SOURCE_DIR}/..)

	add_llext_target(${module}
		OUTPUT  ${PROJECT_BINARY_DIR}/${module}_llext/${module}.llext
		SOURCES ${SOF_LLEXT_SOURCES}
	)

	if(CONFIG_LIBRARY_BUILD_LIB AND NOT("${SOF_LLEXT_LIB}" STREQUAL ""))
		file(WRITE ${PROJECT_BINARY_DIR}/${module}_llext/lib_name.txt ${SOF_LLEXT_LIB})
	endif()

	target_include_directories(${module}_llext_lib PRIVATE
		"${SOF_BASE}/xtos/include"
		"${SOF_BASE}/src/include"
		"${SOF_BASE}/tools/rimage/src/include"
	)

	sof_append_relative_path_definitions(${module}_llext_lib)

	add_llext_command(TARGET ${module}
		PRE_BUILD
		COMMAND ${CMAKE_C_COMPILER} -E ${CMAKE_CURRENT_LIST_DIR}/llext.toml.h -P -DREM=
			-I${SOF_BASE} -I${SOF_BASE}src
			-imacros ../include/generated/zephyr/autoconf.h
			-imacros ../include/generated/uuid-registry.h
			-o rimage_config.toml
	)

	if(CONFIG_LLEXT_TYPE_ELF_RELOCATABLE)
		set(EXTRA_LINKER_PARAMS -nostdlib -nodefaultlibs -r)
	else()
		set(EXTRA_LINKER_PARAMS -nostdlib -nodefaultlibs -shared)
	endif()

	get_target_property(proc_in_file ${module} lib_output)
	get_target_property(proc_out_file ${module} pkg_input)
	get_target_property(proc_pkg_file ${module} pkg_output)
	set(size_file ${PROJECT_BINARY_DIR}/module_size)

	get_property(last_target GLOBAL PROPERTY SOF_LLEXT_LAST_TARGET)
	if(NOT "${last_target}" STREQUAL "")
		add_dependencies(${module}_llext_proc ${last_target})
	endif()
	set_property(GLOBAL PROPERTY SOF_LLEXT_LAST_TARGET ${module})

	add_llext_command(TARGET ${module}
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -DMODULE=${module} -DZEPHYR_BINARY_DIR=${ZEPHYR_BINARY_DIR}
			-P ${SOF_BASE}scripts/cmake/llext_write_uuids.cmake
	)

	add_llext_command(TARGET ${module}
		POST_BUILD
		COMMAND ${PYTHON_EXECUTABLE} ${SOF_BASE}scripts/llext_link_helper.py -s ${size_file}
			--text-addr=${CONFIG_LIBRARY_BASE_ADDRESS}
			-c $<TARGET_PROPERTY:bintools,elfconvert_command>
			-f ${proc_in_file} -o ${proc_out_file} ${CMAKE_C_COMPILER} --
			${EXTRA_LINKER_PARAMS} $<TARGET_OBJECTS:${module}_llext_lib>
	)

	add_llext_command(TARGET ${module}
		POST_PKG
		COMMAND ${PYTHON_EXECUTABLE} ${SOF_BASE}scripts/llext_offset_calc.py
			-i ${proc_pkg_file} -s ${size_file}
	)
endfunction()

# Initial SOF module will contain
#
# 1. Application logic - pipeline, audio components, IPC processing, topology
# 2. IP drivers - SSP, DMIC, PM, IPC will transition to Zephyr directly over
#                 time and be removed from the SOF repo.
# 3. Platform IP - PM, init, clocks, IRQs will transition directly to Zephyr
#                  over time and be removed from SOF repo.
# 4. RTOS logic - scheduler, allocator, notifier - as with 2 & 3.
zephyr_interface_library_named(SOF)

# SOF source paths.
cmake_path(SET sof_top_dir NORMALIZE "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(SOF_SRC_PATH "${sof_top_dir}/src")

set(SOF_PLATFORM_PATH "${SOF_SRC_PATH}/platform")
set(SOF_AUDIO_PATH "${SOF_SRC_PATH}/audio")
set(SOF_SAMPLES_PATH "${SOF_SRC_PATH}/samples")
set(SOF_LIB_PATH "${SOF_SRC_PATH}/lib")
set(SOF_DRIVERS_PATH "${SOF_SRC_PATH}/drivers")
set(SOF_TRACE_PATH "${SOF_SRC_PATH}/trace")

set(RIMAGE_TOP ${sof_top_dir}/tools/rimage)

# Save path to rimage configuration files in cmake cache for later use by
# rimage during the "west sign" stage
set(RIMAGE_CONFIG_PATH ${RIMAGE_TOP}/config CACHE PATH
    " Path to rimage board configuration files")

# default SOF includes
target_include_directories(SOF INTERFACE ${RIMAGE_TOP}/src/include)
target_include_directories(SOF INTERFACE ${SOF_SRC_PATH}/include)
target_include_directories(SOF INTERFACE ${SOF_SRC_PATH}/arch/${ARCH}/include)
target_include_directories(SOF INTERFACE ${sof_top_dir}/third_party/include)
target_include_directories(SOF INTERFACE ${sof_top_dir}/zephyr/include)

# SOF module init
zephyr_library_named(modules_sof)

# Zephyr C++ code requires 14 or newer standard
set_property(TARGET modules_sof PROPERTY CXX_STANDARD 17)

zephyr_include_directories(include)
zephyr_include_directories(${ZEPHYR_BASE}/kernel/include)
zephyr_include_directories(${ZEPHYR_BASE}/arch/${ARCH}/include)

# SOC level sources
# Files that are commented may not be needed.


# New, "de-centralized Zephyr" way. Requires "is_zephyr()" conditionals in
# the decentralized CMakeLists.txt files shared with XTOS.

# XTOS duplicate in sof/scripts/misc.cmake; keep in sync
macro(is_zephyr ret)
	if(CONFIG_ZEPHYR_SOF_MODULE)
		set(${ret} TRUE)
	else()
		set(${ret} FALSE)
	endif()
endmacro()

# Wrappers for compatibility and re-use of existing, XTOS CMake files.
# Do NOT use these macros in this file or in any other Zephyr-specific
# CMake code.
macro(add_local_sources target)
	if (NOT "${target}" STREQUAL "sof")
		message(FATAL_ERROR "add_local_sources() target is not 'sof'")
	endif()
	zephyr_library_sources(${ARGN})
endmacro()
macro(add_local_sources_ifdef condition target)
	if (NOT "${target}" STREQUAL "sof")
		message(FATAL_ERROR "add_local_sources_ifdef() target is not 'sof'")
	endif()
	zephyr_library_sources_ifdef(${condition} ${ARGN})
endmacro()
macro(sof_list_append_ifdef feature_toggle list)
  if(${${feature_toggle}})
    list(APPEND ${list} ${ARGN})
  endif()
endmacro()

# directories with auxiliary modules need to be listed first
# - llext dependencies need to added to linkage first
add_subdirectory(../src/math/  math_unused_install/)

# remaining directories (in alphabetical order)
add_subdirectory(../src/audio/ audio_unused_install/)
add_subdirectory(../src/debug/ debug_unused_install/)
add_subdirectory(../src/init/ init_unused_install/)
add_subdirectory(../src/ipc/  ipc_unused_install/)
add_subdirectory(../src/schedule/  schedule_unused_install/)
add_subdirectory(test/)


# Old way below: all .c files added by this giant CMake file.

# Intel TGL and CAVS 2.5 platforms
if (CONFIG_SOC_SERIES_INTEL_CAVS_V25)

	# Driver sources
	zephyr_library_sources_ifdef(CONFIG_INTEL_HDA
		${SOF_DRIVERS_PATH}/intel/hda/hda-dma.c
	)

	# Platform sources
	zephyr_library_sources(
		${SOF_PLATFORM_PATH}/intel/cavs/platform.c
		lib/dma.c
	)

	set(PLATFORM "tigerlake")
	zephyr_include_directories(${SOF_PLATFORM_PATH}/intel/cavs/include)
endif()

# Intel ACE 1.5 and newer platforms
if (CONFIG_SOC_SERIES_INTEL_ADSP_ACE)

	# Platform sources
	zephyr_library_sources(
		${SOF_PLATFORM_PATH}/intel/ace/platform.c
		lib/clk.c
		lib/dma.c
	)

	zephyr_library_sources_ifdef(CONFIG_SOC_INTEL_ACE15_MTPM
			${SOF_PLATFORM_PATH}/meteorlake/lib/clk.c
		)

	zephyr_library_sources_ifdef(CONFIG_SOC_INTEL_ACE20_LNL
			${SOF_PLATFORM_PATH}/lunarlake/lib/clk.c
		)

	zephyr_library_sources_ifdef(CONFIG_SOC_INTEL_ACE30
			${SOF_PLATFORM_PATH}/ace30/lib/clk.c
		)

	# SOF core infrastructure - runs on top of Zephyr
	zephyr_library_sources(
		${SOF_SRC_PATH}/schedule/zephyr_ll.c
	)

	# Sources for virtual heap management
	zephyr_library_sources(
		lib/regions_mm.c
	)

	zephyr_library_sources_ifdef(CONFIG_CAVS_LPS
		${SOF_PLATFORM_PATH}/intel/ace/lps_wait.c
	)

	zephyr_library_sources_ifdef(CONFIG_LL_WATCHDOG
		${SOF_PLATFORM_PATH}/intel/ace/lib/watchdog.c
	)

	if (CONFIG_SOC_INTEL_ACE15_MTPM)
		set(PLATFORM "meteorlake")
	elseif(CONFIG_SOC_INTEL_ACE20_LNL)
		set(PLATFORM "lunarlake")
	elseif(CONFIG_SOC_INTEL_ACE30)
		set(PLATFORM "ace30")
	endif()

	zephyr_include_directories(${SOF_PLATFORM_PATH}/intel/ace/include)
	zephyr_include_directories(${SOF_PLATFORM_PATH}/${PLATFORM}/include)
endif()

# NXP IMX8 platforms
if (CONFIG_SOC_MIMX8QM6_ADSP OR CONFIG_SOC_MIMX8QX6_ADSP)
	zephyr_library_sources(
		${SOF_DRIVERS_PATH}/imx/ipc.c
	)

	# Platform sources
	zephyr_library_sources(
		${SOF_PLATFORM_PATH}/imx8/platform.c
		${SOF_PLATFORM_PATH}/imx8/lib/clk.c
	)

	# SOF core infrastructure - runs on top of Zephyr
	zephyr_library_sources(
		lib/dma.c
	)

	set(PLATFORM "imx8")
endif()

if (CONFIG_SOC_MIMX8ML8_ADSP)
	zephyr_library_sources(
		${SOF_DRIVERS_PATH}/imx/ipc.c
	)

	# Platform sources
	zephyr_library_sources(
		${SOF_PLATFORM_PATH}/imx8m/platform.c
		${SOF_PLATFORM_PATH}/imx8m/lib/clk.c
	)

	# SOF core infrastructure - runs on top of Zephyr
	zephyr_library_sources(
		lib/dma.c
	)

	set(PLATFORM "imx8m")
endif()

if (CONFIG_SOC_MIMX8UD7_ADSP)
	zephyr_library_sources(
		${SOF_DRIVERS_PATH}/imx/ipc.c
	)

	# Platform sources
	zephyr_library_sources(
		${SOF_PLATFORM_PATH}/imx8ulp/platform.c
		${SOF_PLATFORM_PATH}/imx8ulp/lib/clk.c
	)

	# SOF core infrastructure - runs on top of Zephyr
	zephyr_library_sources(
		lib/dma.c
	)

	set(PLATFORM "imx8ulp")
endif()

if (CONFIG_SOC_MIMX9352_A55)
	# Platform sources
	zephyr_library_sources(
		${SOF_PLATFORM_PATH}/imx93_a55/platform.c
		${SOF_PLATFORM_PATH}/imx93_a55/lib/clk.c
		lib/dma.c
	)

	# Drivers
	zephyr_library_sources(
		${SOF_DRIVERS_PATH}/imx/ipc.c
	)

	set(PLATFORM "imx93_a55")
endif()

if (CONFIG_SOC_MIMX9596_M7)

	zephyr_library_sources(
		${SOF_PLATFORM_PATH}/imx95/platform.c
		${SOF_PLATFORM_PATH}/imx95/lib/clk.c
		lib/dma.c
	)

	zephyr_library_sources(
		${SOF_DRIVERS_PATH}/imx/ipc.c
	)

	# SOF-specific linker script additions
	zephyr_linker_sources(DATA_SECTIONS ${sof_top_dir}/src/platform/imx95/linker/data-sections.ld)

	set(PLATFORM "imx95")
endif()

# AMD RMB platforms
if (CONFIG_SOC_ACP_6_0)
    zephyr_library_sources(
		${SOF_DRIVERS_PATH}/amd/common/acp_bt_dai.c
		${SOF_DRIVERS_PATH}/amd/common/acp_dma.c
		${SOF_DRIVERS_PATH}/amd/common/acp_dmic_dai.c
		${SOF_DRIVERS_PATH}/amd/common/acp_dmic_dma.c
		${SOF_DRIVERS_PATH}/amd/common/acp_sp_dma.c
		${SOF_DRIVERS_PATH}/amd/common/ipc.c
		${SOF_DRIVERS_PATH}/amd/rembrandt/acp_bt_dma.c
		${SOF_DRIVERS_PATH}/amd/rembrandt/acp_dma.c
		${SOF_DRIVERS_PATH}/amd/rembrandt/acp_dmic_dma.c
		${SOF_DRIVERS_PATH}/amd/rembrandt/acp_hs_dai.c
		${SOF_DRIVERS_PATH}/amd/rembrandt/acp_hs_dma.c
		${SOF_DRIVERS_PATH}/amd/rembrandt/acp_sp_dai.c
		${SOF_DRIVERS_PATH}/amd/rembrandt/acp_sp_dma.c
		${SOF_DRIVERS_PATH}/amd/rembrandt/interrupt.c
		${SOF_DRIVERS_PATH}/amd/rembrandt/ipc.c
	)

	# Platform sources
	zephyr_library_sources(
		${SOF_PLATFORM_PATH}/amd/rembrandt/platform.c
		${SOF_PLATFORM_PATH}/amd/rembrandt/lib/clk.c
		${SOF_PLATFORM_PATH}/amd/rembrandt/lib/dai.c
		${SOF_PLATFORM_PATH}/amd/rembrandt/lib/dma.c
		${SOF_PLATFORM_PATH}/amd/rembrandt/lib/memory.c
	)

	# SOF core infrastructure - runs on top of Zephyr
	zephyr_library_sources(
		${SOF_SRC_PATH}/drivers/interrupt.c
	)

	zephyr_include_directories(${SOF_PLATFORM_PATH}/amd/rembrandt/include)
	zephyr_include_directories(${SOF_PLATFORM_PATH}/amd/common/include)
	zephyr_include_directories(${SOF_PLATFORM_PATH}/amd/rembrandt/include/arch)

	set(PLATFORM "acp_6_0")
endif()

if (CONFIG_SOC_FAMILY_MTK)
	set(PLATFORM "mtk")

	zephyr_library_sources(
		${SOF_PLATFORM_PATH}/mtk/platform.c
		${SOF_PLATFORM_PATH}/mtk/dai.c
		${SOF_DRIVERS_PATH}/generic/dummy-dma.c
		${SOF_SRC_PATH}/drivers/mediatek/afe/afe-memif.c
		${SOF_SRC_PATH}/drivers/mediatek/afe/afe-dai.c
		${SOF_SRC_PATH}/drivers/mediatek/afe/afe-drv.c
	)
endif()

# Building for native_posix-based whole-OS host emulator
zephyr_library_sources_ifdef(CONFIG_ZEPHYR_POSIX
	${SOF_PLATFORM_PATH}/posix/dma.c
	${SOF_PLATFORM_PATH}/posix/dai.c
	${SOF_PLATFORM_PATH}/posix/ipc.c
	${SOF_PLATFORM_PATH}/posix/posix.c
	${SOF_PLATFORM_PATH}/posix/fuzz.c
)

zephyr_library_sources_ifdef(CONFIG_LIBRARY
	${SOF_PLATFORM_PATH}/library/platform.c
	${SOF_PLATFORM_PATH}/library/lib/dai.c
	${SOF_DRIVERS_PATH}/host/ipc.c
)

if(NOT DEFINED PLATFORM)
	message(FATAL_ERROR "PLATFORM is not defined, check your Kconfiguration?")
endif()
zephyr_include_directories(${SOF_PLATFORM_PATH}/${PLATFORM}/include)

# Mandatory Files used on all platforms.
# Commented files will be added/removed as integration dictates.
zephyr_library_sources(

	# SOF library - parts to transition to Zephyr over time
	${SOF_LIB_PATH}/notifier.c
	${SOF_LIB_PATH}/dma.c
	${SOF_LIB_PATH}/dai.c

	# SOF core infrastructure - runs on top of Zephyr
	${SOF_SRC_PATH}/arch/xtensa/drivers/cache_attr.c

	${SOF_SRC_PATH}/idc/zephyr_idc.c

	# Bridge wrapper between SOF and Zephyr APIs - Will shrink over time.
	wrapper.c
	edf_schedule.c
	schedule.c
	lib/alloc.c
	lib/cpu.c
	lib/pm_runtime.c

	# Common library functions - Will be moved to Zephyr over time
	lib.c
)

if(NOT CONFIG_SOF_ZEPHYR_NO_SOF_CLOCK)
	zephyr_library_sources(${SOF_LIB_PATH}/clk.c)
endif()

zephyr_library_sources_ifdef(CONFIG_KCPS_DYNAMIC_CLOCK_CONTROL
	${SOF_LIB_PATH}/cpu-clk-manager.c
)

# SOF module interface functions
add_subdirectory(../src/module module_unused_install/)

zephyr_library_sources_ifdef(CONFIG_TRACE
	${SOF_SRC_PATH}/trace/dma-trace.c
	${SOF_SRC_PATH}/trace/trace.c)

zephyr_library_sources_ifdef(CONFIG_LOG_BACKEND_SOF_PROBE
      ${SOF_SRC_PATH}/logging/log_backend_probe.c)

zephyr_library_sources_ifdef(CONFIG_FAST_GET lib/fast-get.c)

# Optional SOF sources - depends on Kconfig - WIP

if(CONFIG_IPC_MAJOR_3)
set(ipc_suffix ipc3)
elseif(CONFIG_IPC_MAJOR_4)
set(ipc_suffix ipc4)
endif()

zephyr_library_sources_ifdef(CONFIG_SAMPLE_KEYPHRASE
	${SOF_SAMPLES_PATH}/audio/detect_test.c
)

zephyr_library_sources_ifdef(CONFIG_LIBRARY_MANAGER
	${SOF_SRC_PATH}/library_manager/lib_manager.c
	${SOF_SRC_PATH}/library_manager/lib_notification.c
)

if (CONFIG_MM_DRV AND CONFIG_LLEXT)
zephyr_library_sources_ifdef(CONFIG_LIBRARY_MANAGER
	${SOF_SRC_PATH}/library_manager/llext_manager.c
)
endif()

zephyr_library_sources_ifdef(CONFIG_MAXIM_DSM
	${SOF_AUDIO_PATH}/smart_amp/smart_amp.c
	${SOF_AUDIO_PATH}/smart_amp/smart_amp_generic.c
	${SOF_AUDIO_PATH}/smart_amp/smart_amp_maxim_dsm.c
)

zephyr_include_directories_ifdef(CONFIG_MAXIM_DSM
	${SOF_AUDIO_PATH}/smart_amp/include/dsm_api/inc/
)

zephyr_library_sources_ifdef(CONFIG_MAXIM_DSM_STUB
	${SOF_AUDIO_PATH}/smart_amp/maxim_dsm_stub.c
)

if(CONFIG_COMP_GOOGLE_RTC_AUDIO_PROCESSING STREQUAL "m")
	add_subdirectory(${SOF_AUDIO_PATH}/google/llext_rtc
			 ${PROJECT_BINARY_DIR}/google_rtc_audio_processing_llext)
	add_dependencies(app google_rtc_audio_processing)
elseif(CONFIG_COMP_GOOGLE_RTC_AUDIO_PROCESSING)
	zephyr_library_sources(
		${SOF_AUDIO_PATH}/google/google_rtc_audio_processing.c
	)

	zephyr_library_sources_ifdef(CONFIG_GOOGLE_RTC_AUDIO_PROCESSING_MOCK
		${SOF_AUDIO_PATH}/google/google_rtc_audio_processing_mock.c
	)

	if(NOT CONFIG_GOOGLE_RTC_AUDIO_PROCESSING_MOCK)
		zephyr_include_directories(../third_party/include)
		target_link_directories(SOF INTERFACE ../third_party/lib)
		target_link_libraries(SOF INTERFACE google_rtc_audio_processing)
		target_link_libraries(SOF INTERFACE c++)
		target_link_libraries(SOF INTERFACE c++abi)
		target_link_libraries(SOF INTERFACE m)
		target_link_libraries(SOF INTERFACE c)
		target_link_libraries(SOF INTERFACE gcc)
	endif()
endif()

if(CONFIG_COMP_GOOGLE_CTC_AUDIO_PROCESSING STREQUAL "m")
	add_subdirectory(${SOF_AUDIO_PATH}/google/llext_ctc
			 ${PROJECT_BINARY_DIR}/google_ctc_audio_processing_llext)
	add_dependencies(app google_ctc_audio_processing)
elseif(CONFIG_COMP_GOOGLE_CTC_AUDIO_PROCESSING)
	zephyr_library_sources(
		${SOF_AUDIO_PATH}/google/google_ctc_audio_processing.c
		${SOF_AUDIO_PATH}/google/google_ctc_audio_processing_${ipc_suffix}.c
	)

	zephyr_library_sources_ifdef(CONFIG_GOOGLE_CTC_AUDIO_PROCESSING_MOCK
		${SOF_AUDIO_PATH}/google/google_ctc_audio_processing_mock.c
	)

	if(NOT CONFIG_GOOGLE_CTC_AUDIO_PROCESSING_MOCK)
		zephyr_include_directories(../third_party/include)
		target_link_directories(SOF INTERFACE ../third_party/lib)
		target_link_libraries(SOF INTERFACE google_ctc_audio_processing)
		target_link_libraries(SOF INTERFACE c++)
		target_link_libraries(SOF INTERFACE c++abi)
		target_link_libraries(SOF INTERFACE m)
		target_link_libraries(SOF INTERFACE c)
		target_link_libraries(SOF INTERFACE gcc)
	endif()
endif()

if(CONFIG_COMP_IGO_NR STREQUAL "m")
	add_subdirectory(${SOF_AUDIO_PATH}/igo_nr/llext
			 ${PROJECT_BINARY_DIR}/igo_nr_llext)
	add_dependencies(app igo_nr)
elseif(CONFIG_COMP_IGO_NR)
	zephyr_library_sources(
		${SOF_AUDIO_PATH}/igo_nr/igo_nr.c
	)

	zephyr_library_sources_ifdef(CONFIG_COMP_IGO_NR_STUB
		${SOF_AUDIO_PATH}/igo_nr/igo_nr_stub.c
	)
endif()

if(CONFIG_COMP_RTNR STREQUAL "m")
	add_subdirectory(${SOF_AUDIO_PATH}/rtnr/llext
			 ${PROJECT_BINARY_DIR}/rtnr_llext)
	add_dependencies(app rtnr)
elseif(CONFIG_COMP_RTNR)
	zephyr_library_sources(
		${SOF_AUDIO_PATH}/rtnr/rtnr.c
	)

	zephyr_library_sources_ifdef(CONFIG_COMP_RTNR_STUB
		${SOF_AUDIO_PATH}/rtnr/rtnr_stub.c
	)
endif()

if(CONFIG_SAMPLE_SMART_AMP STREQUAL "m")
	add_subdirectory(${SOF_SAMPLES_PATH}/audio/smart_amp_test_llext
			 ${PROJECT_BINARY_DIR}/smart_amp_test_llext)
	add_dependencies(app smart_amp_test)
elseif(CONFIG_SAMPLE_SMART_AMP)
	zephyr_library_sources(
		${SOF_SAMPLES_PATH}/audio/smart_amp_test_${ipc_suffix}.c
	)
endif()

zephyr_library_sources_ifdef(CONFIG_COMP_GOOGLE_HOTWORD_DETECT
	${SOF_AUDIO_PATH}/google/google_hotword_detect.c
)

if(CONFIG_PROBE STREQUAL "m")
	add_subdirectory(${SOF_SRC_PATH}/probe/llext
			 ${PROJECT_BINARY_DIR}/probe_llext)
	add_dependencies(app probe)
elseif(CONFIG_PROBE)
	zephyr_library_sources(${SOF_SRC_PATH}/probe/probe.c)
endif()

zephyr_library_sources_ifdef(CONFIG_MULTICORE
	${SOF_SRC_PATH}/idc/idc.c
)

zephyr_library_sources_ifdef(CONFIG_HAVE_AGENT
	${SOF_LIB_PATH}/agent.c
)

zephyr_library_sources_ifdef(CONFIG_AMS
	${SOF_LIB_PATH}/ams.c
)

zephyr_library_sources_ifdef(CONFIG_DW_DMA
	${SOF_DRIVERS_PATH}/dw/dma.c
)

zephyr_library_sources_ifdef(CONFIG_SOF_BOOT_TEST
       boot_test.c
)

zephyr_library_sources_ifdef(CONFIG_SHELL
       sof_shell.c
)

zephyr_library_sources_ifdef(CONFIG_INTEL_ADSP_MIC_PRIVACY
	${SOF_AUDIO_PATH}/mic_privacy_manager/mic_privacy_manager_intel.c
)

zephyr_library_link_libraries(SOF)
target_link_libraries(SOF INTERFACE zephyr_interface)

# Linker snippet for the UUID table
zephyr_linker_sources("ROM_SECTIONS" uuid-snippet.ld)

# Setup SOF directories
set(SOF_ROOT_SOURCE_DIRECTORY ${sof_top_dir})
set(SOF_ROOT_BINARY_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

# This generated/ directory is shared with Zephyr.
# PROJECT_BINARY_DIR is build/zephyr/
set(GENERATED_DIRECTORY ${PROJECT_BINARY_DIR}/include/generated)

set(VERSION_H_PATH ${GENERATED_DIRECTORY}/sof_versions.h)

find_package(Python3 COMPONENTS Interpreter)
set(PYTHON3 "${Python3_EXECUTABLE}")

if (NOT CONFIG_COMPILER_INLINE_FUNCTION_OPTION)
target_compile_options(SOF INTERFACE -fno-inline-functions)
endif()

# SOF needs `typeof`, `__VA_ARGS__` and maybe other GNU C99
# extensions. TODO other flags required ?
target_compile_options(SOF INTERFACE $<$<COMPILE_LANGUAGE:C,ASM>: -std=gnu99>)

# Toolchain info
add_definitions(-DXCC_TOOLS_VERSION="${ZEPHYR_TOOLCHAIN_VARIANT}" -DCC_OPTIMIZE_FLAGS="${OPTIMIZATION_FLAG}")

# create version information
include(../scripts/cmake/version.cmake)

include(../scripts/cmake/uuid-registry.cmake)

# Create Trace realtive file paths
sof_append_relative_path_definitions(modules_sof)

endif() # CONFIG_SOF
