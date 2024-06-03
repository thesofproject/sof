# When west is installed, Zephyr's CMake invokes west to list and try to
# compile every Zephyr module that can be found. See
# sof/zephyr/module.yml and
# https://docs.zephyrproject.org/latest/develop/modules.html
if(CONFIG_SOF)

if(CONFIG_LIBRARY)
	set(PLATFORM "library")
	set(ARCH host)
	zephyr_include_directories(../src/platform/library/include)
elseif(CONFIG_ZEPHYR_POSIX)
	set(ARCH host)
	set(PLATFORM "posix")
else()
	# firmware build supports only xtensa arch for now
	set(ARCH xtensa)
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

# Used by LLEXT modules to create a file with module UUIDs
function(sof_llext_write_uuids module)
	file(STRINGS ${CMAKE_CURRENT_LIST_DIR}/../${module}.toml uuids REGEX "^[ \t]*uuid *=")

	set(UUIDS_LIST_FILE ${ZEPHYR_BINARY_DIR}/${module}_llext/llext.uuid)
	file(REMOVE ${UUIDS_LIST_FILE})
	foreach(line IN LISTS uuids)
		# extract UUID value - drop the 'uuid = ' part of the assignment line
		string(REGEX REPLACE "^[ \t]*uuid *= \"([0-9A-F\-]*)\"" "\\1" uuid ${line})
		file(APPEND ${UUIDS_LIST_FILE} "${uuid}\n")
	endforeach()
endfunction()

# Build an LLEXT module. Provice a module name, a list of sources and an address
# of the .text section as arguments.
function(sof_llext_build module)
	set(single_args TEXT_ADDR)
	set(multi_args SOURCES)
	cmake_parse_arguments(PARSE_ARGV 1 SOF_LLEXT "${options}" "${single_args}" "${multi_args}")

	cmake_path(SET SOF_BASE NORMALIZE ${APPLICATION_SOURCE_DIR}/..)

	sof_llext_write_uuids(${module})

	add_llext_target(${module}
		OUTPUT  ${PROJECT_BINARY_DIR}/${module}_llext/${module}.llext
		SOURCES ${SOF_LLEXT_SOURCES}
	)

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
			-o rimage_config.toml
	)

	if(CONFIG_LLEXT_TYPE_ELF_RELOCATABLE)
		set(EXTRA_LINKER_PARAMS -nostdlib -nodefaultlibs -r)
	else()
		set(EXTRA_LINKER_PARAMS -nostdlib -nodefaultlibs -shared)
	endif()

	get_target_property(proc_in_file ${module} lib_output)
	get_target_property(proc_out_file ${module} pkg_input)
	add_llext_command(TARGET ${module}
		POST_BUILD
		COMMAND ${PYTHON_EXECUTABLE} ${SOF_BASE}scripts/llext_link_helper.py
			--text-addr="${SOF_LLEXT_TEXT_ADDR}" -f ${proc_in_file} ${CMAKE_C_COMPILER} --
			-o ${proc_out_file} ${EXTRA_LINKER_PARAMS}
	                $<TARGET_OBJECTS:${module}_llext_lib>
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
set(SOF_AUDIO_MODULES_PATH "${SOF_SRC_PATH}/audio/module_adapter/module")
set(SOF_SAMPLES_PATH "${SOF_SRC_PATH}/samples")
set(SOF_LIB_PATH "${SOF_SRC_PATH}/lib")
set(SOF_DRIVERS_PATH "${SOF_SRC_PATH}/drivers")
set(SOF_DEBUG_PATH "${SOF_SRC_PATH}/debug")
set(SOF_MATH_PATH "${SOF_SRC_PATH}/math")
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

# TODO: Zephyr should not need xtos headers: FIX.
if (CONFIG_SOF_ZEPHYR_STRICT_HEADERS)
	target_include_directories(SOF INTERFACE ${sof_top_dir}/zephyr/include)
else()
	# include Zephyr before xtos to flag up any errors in SOF
	target_include_directories(SOF INTERFACE ${sof_top_dir}/zephyr/include)
	target_include_directories(SOF INTERFACE ${sof_top_dir}/xtos/include)
endif()

# SOF module init
zephyr_library_named(modules_sof)

# Zephyr C++ code requires 14 or newer standard
set_property(TARGET modules_sof PROPERTY CXX_STANDARD 17)

zephyr_include_directories(
	include
)

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

add_subdirectory(../src/init/ init_unused_install/)
add_subdirectory(../src/ipc/  ipc_unused_install/)
add_subdirectory(../src/debug/telemetry/  telemetry_unused_install/)
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
		${SOF_PLATFORM_PATH}/tigerlake/lib/clk.c
		lib/pm_runtime.c
		lib/clk.c
		lib/dma.c
	)

	# SOF core infrastructure - runs on top of Zephyr
	zephyr_library_sources(
		${SOF_SRC_PATH}/schedule/zephyr_ll.c
	)

	set(PLATFORM "tigerlake")
	zephyr_include_directories(${SOF_PLATFORM_PATH}/intel/cavs/include)
endif()

# Intel ACE 1.5 and newer platforms
if (CONFIG_SOC_SERIES_INTEL_ADSP_ACE)

	# Platform sources
	zephyr_library_sources(
		${SOF_PLATFORM_PATH}/intel/ace/platform.c
		lib/pm_runtime.c
		lib/clk.c
		lib/dma.c
	)

	zephyr_library_sources_ifdef(CONFIG_SOC_INTEL_ACE15_MTPM
			${SOF_PLATFORM_PATH}/meteorlake/lib/clk.c
		)

	zephyr_library_sources_ifdef(CONFIG_SOC_INTEL_ACE20_LNL
			${SOF_PLATFORM_PATH}/lunarlake/lib/clk.c
		)

	zephyr_library_sources_ifdef(CONFIG_SOC_INTEL_ACE30_PTL
			${SOF_PLATFORM_PATH}/pantherlake/lib/clk.c
		)

	# SOF core infrastructure - runs on top of Zephyr
	zephyr_library_sources(
		${SOF_SRC_PATH}/schedule/zephyr_ll.c
	)

	zephyr_library_sources_ifdef(CONFIG_ZEPHYR_DP_SCHEDULER
		${SOF_SRC_PATH}/schedule/zephyr_dp_schedule.c
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
	elseif(CONFIG_SOC_INTEL_ACE30_PTL)
		set(PLATFORM "pantherlake")
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
		${SOF_PLATFORM_PATH}/imx8/lib/memory.c
	)

	# SOF core infrastructure - runs on top of Zephyr
	zephyr_library_sources(
		lib/dma.c
	)

	zephyr_library_sources(${SOF_SRC_PATH}/schedule/zephyr_ll.c)

	set(PLATFORM "imx8")
endif()

if (CONFIG_SOC_MIMX8ML8_ADSP)
	zephyr_library_sources(
		${SOF_DRIVERS_PATH}/generic/dummy-dma.c
		${SOF_DRIVERS_PATH}/imx/sdma.c
		${SOF_DRIVERS_PATH}/imx/sai.c
		${SOF_DRIVERS_PATH}/imx/ipc.c
		${SOF_DRIVERS_PATH}/imx/micfil.c
		${SOF_DRIVERS_PATH}/imx/interrupt-irqsteer.c
	)

	# Platform sources
	zephyr_library_sources(
		${SOF_PLATFORM_PATH}/imx8m/platform.c
		${SOF_PLATFORM_PATH}/imx8m/lib/clk.c
		${SOF_PLATFORM_PATH}/imx8m/lib/memory.c
		${SOF_PLATFORM_PATH}/imx8m/lib/dai.c
		${SOF_PLATFORM_PATH}/imx8m/lib/dma.c
	)

	# SOF core infrastructure - runs on top of Zephyr
	zephyr_library_sources(
		${SOF_SRC_PATH}/drivers/interrupt.c
	)

	zephyr_library_sources(${SOF_SRC_PATH}/schedule/zephyr_ll.c)

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
		${SOF_PLATFORM_PATH}/imx8ulp/lib/memory.c
	)

	# SOF core infrastructure - runs on top of Zephyr
	zephyr_library_sources(
		lib/dma.c
	)

	zephyr_library_sources(${SOF_SRC_PATH}/schedule/zephyr_ll.c)

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

	zephyr_library_sources(
		${SOF_SRC_PATH}/schedule/zephyr_ll.c
	)

	set(PLATFORM "imx93_a55")
endif()

# Building for native_posix-based whole-OS host emulator
zephyr_library_sources_ifdef(CONFIG_ZEPHYR_POSIX
	${SOF_SRC_PATH}/schedule/zephyr_ll.c
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

	# SOF math utilities
	${SOF_MATH_PATH}/decibels.c
	${SOF_MATH_PATH}/numbers.c
	${SOF_MATH_PATH}/trig.c
	${SOF_MATH_PATH}/exp_fcn.c
	${SOF_MATH_PATH}/exp_fcn_hifi.c

	# SOF library - parts to transition to Zephyr over time
	${SOF_LIB_PATH}/clk.c
	${SOF_LIB_PATH}/notifier.c
	${SOF_LIB_PATH}/pm_runtime.c
	${SOF_LIB_PATH}/cpu-clk-manager.c
	${SOF_LIB_PATH}/dma.c
	${SOF_LIB_PATH}/dai.c

	# SOF mandatory audio processing
	${SOF_AUDIO_PATH}/channel_map.c
	${SOF_AUDIO_PATH}/pcm_converter/pcm_converter_hifi3.c
	${SOF_AUDIO_PATH}/pcm_converter/pcm_converter.c
	${SOF_AUDIO_PATH}/pcm_converter/pcm_converter_generic.c
	${SOF_AUDIO_PATH}/buffer.c
	${SOF_AUDIO_PATH}/source_api_helper.c
	${SOF_AUDIO_PATH}/sink_api_helper.c
	${SOF_AUDIO_PATH}/sink_source_utils.c
	${SOF_AUDIO_PATH}/audio_stream.c
	${SOF_AUDIO_PATH}/component.c
	${SOF_AUDIO_PATH}/pipeline/pipeline-graph.c
	${SOF_AUDIO_PATH}/pipeline/pipeline-params.c
	${SOF_AUDIO_PATH}/pipeline/pipeline-schedule.c
	${SOF_AUDIO_PATH}/pipeline/pipeline-stream.c
	${SOF_AUDIO_PATH}/pipeline/pipeline-xrun.c

	# SOF core infrastructure - runs on top of Zephyr
	${SOF_SRC_PATH}/arch/xtensa/drivers/cache_attr.c
	${SOF_SRC_PATH}/schedule/zephyr_domain.c
	${SOF_SRC_PATH}/schedule/schedule.c

	${SOF_SRC_PATH}/idc/zephyr_idc.c

	# Bridge wrapper between SOF and Zephyr APIs - Will shrink over time.
	wrapper.c
	edf_schedule.c
	schedule.c
	lib/alloc.c
	lib/cpu.c

	# Common library functions - Will be moved to Zephyr over time
	lib.c
)

# Optional math utility
zephyr_library_sources_ifdef(CONFIG_MATH_LUT_SINE_FIXED
	${SOF_MATH_PATH}/lut_trig.c
)

# SOF module interface functions
add_subdirectory(../src/module module_unused_install/)

if(CONFIG_PIPELINE_2_0)
	zephyr_library_sources(${SOF_AUDIO_PATH}/buffers/ring_buffer.c)
endif()
if(CONFIG_SCHEDULE_DMA_SINGLE_CHANNEL AND NOT(CONFIG_DMA_DOMAIN))
	zephyr_library_sources(${SOF_SRC_PATH}/schedule/dma_single_chan_domain.c)
endif()
if(CONFIG_SCHEDULE_DMA_MULTI_CHANNEL AND NOT(CONFIG_DMA_DOMAIN))
	zephyr_library_sources(${SOF_SRC_PATH}/schedule/dma_multi_chan_domain.c)
endif()
if (CONFIG_DMA_DOMAIN)
	zephyr_library_sources(${SOF_SRC_PATH}/schedule/zephyr_dma_domain.c)
endif()

if(CONFIG_COMP_BLOB)
	zephyr_library_sources(
		${SOF_AUDIO_PATH}/data_blob.c
	)
endif()

if(CONFIG_ZEPHYR_NATIVE_DRIVERS)
	zephyr_library_sources(
		${SOF_AUDIO_PATH}/host-zephyr.c
	)
else()
	zephyr_library_sources(
		${SOF_AUDIO_PATH}/host-legacy.c
	)
endif()


zephyr_library_sources_ifdef(CONFIG_TRACE
	${SOF_SRC_PATH}/trace/dma-trace.c
	${SOF_SRC_PATH}/trace/trace.c)

zephyr_library_sources_ifdef(CONFIG_LOG_BACKEND_SOF_PROBE
      ${SOF_SRC_PATH}/logging/log_backend_probe.c)

# Optional SOF sources - depends on Kconfig - WIP

if(CONFIG_IPC_MAJOR_3)
set(ipc_suffix ipc3)
elseif(CONFIG_IPC_MAJOR_4)
set(ipc_suffix ipc4)
endif()

zephyr_library_sources_ifdef(CONFIG_COMP_FIR
	${SOF_AUDIO_PATH}/eq_fir/eq_fir_hifi3.c
	${SOF_AUDIO_PATH}/eq_fir/eq_fir_hifi2ep.c
	${SOF_AUDIO_PATH}/eq_fir/eq_fir_generic.c
	${SOF_AUDIO_PATH}/eq_fir/eq_fir.c
	${SOF_AUDIO_PATH}/eq_fir/eq_fir_${ipc_suffix}.c
)

if(CONFIG_COMP_IIR STREQUAL "m")
	add_subdirectory(${SOF_AUDIO_PATH}/eq_iir/llext
			 ${PROJECT_BINARY_DIR}/eq_iir_llext)
	add_dependencies(app eq_iir)
elseif(CONFIG_COMP_IIR)
	zephyr_library_sources(
		${SOF_AUDIO_PATH}/eq_iir/eq_iir.c
		${SOF_AUDIO_PATH}/eq_iir/eq_iir_${ipc_suffix}.c
		${SOF_AUDIO_PATH}/eq_iir/eq_iir_generic.c
	)
endif()

zephyr_library_sources_ifdef(CONFIG_MATH_FIR
	${SOF_MATH_PATH}/fir_generic.c
	${SOF_MATH_PATH}/fir_hifi2ep.c
	${SOF_MATH_PATH}/fir_hifi3.c
)

zephyr_library_sources_ifdef(CONFIG_MATH_IIR_DF1
	${SOF_MATH_PATH}/iir_df1_generic.c
	${SOF_MATH_PATH}/iir_df1_hifi3.c
	${SOF_MATH_PATH}/iir_df1.c
)

zephyr_library_sources_ifdef(CONFIG_MATH_IIR_DF2T
	${SOF_MATH_PATH}/iir_df2t_generic.c
	${SOF_MATH_PATH}/iir_df2t_hifi3.c
	${SOF_MATH_PATH}/iir_df2t.c
)

zephyr_library_sources_ifdef(CONFIG_COMP_ASRC
	${SOF_AUDIO_PATH}/asrc/asrc.c
	${SOF_AUDIO_PATH}/asrc/asrc_farrow_hifi3.c
	${SOF_AUDIO_PATH}/asrc/asrc_farrow.c
	${SOF_AUDIO_PATH}/asrc/asrc_farrow_generic.c
)

if(CONFIG_COMP_ASRC)
	zephyr_library_sources(${SOF_AUDIO_PATH}/asrc/asrc_${ipc_suffix}.c)
endif()

zephyr_library_sources_ifdef(CONFIG_COMP_DCBLOCK
	${SOF_AUDIO_PATH}/dcblock/dcblock_generic.c
	${SOF_AUDIO_PATH}/dcblock/dcblock.c
	${SOF_AUDIO_PATH}/dcblock/dcblock_hifi3.c
	${SOF_AUDIO_PATH}/dcblock/dcblock_hifi4.c
)

zephyr_library_sources_ifdef(CONFIG_COMP_DCBLOCK
	${SOF_AUDIO_PATH}/dcblock/dcblock_${ipc_suffix}.c
)

zephyr_library_sources_ifdef(CONFIG_COMP_SEL
	${SOF_AUDIO_PATH}/selector/selector_generic.c
	${SOF_AUDIO_PATH}/selector/selector.c
)

zephyr_library_sources_ifdef(CONFIG_COMP_KPB
	${SOF_AUDIO_PATH}/kpb.c
)

zephyr_library_sources_ifdef(CONFIG_COMP_MIXER
	${SOF_AUDIO_PATH}/mixer/mixer.c
	${SOF_AUDIO_PATH}/mixer/mixer_generic.c
	${SOF_AUDIO_PATH}/mixer/mixer_hifi3.c
)

if(CONFIG_COMP_MIXIN_MIXOUT STREQUAL "m")
	add_subdirectory(${SOF_AUDIO_PATH}/mixin_mixout/llext
			 ${PROJECT_BINARY_DIR}/mixin_mixout_llext)
	add_dependencies(app mixin_mixout)
elseif(CONFIG_COMP_MIXIN_MIXOUT)
	zephyr_library_sources(
		${SOF_AUDIO_PATH}/mixin_mixout/mixin_mixout.c
		${SOF_AUDIO_PATH}/mixin_mixout/mixin_mixout_generic.c
		${SOF_AUDIO_PATH}/mixin_mixout/mixin_mixout_hifi3.c
		${SOF_AUDIO_PATH}/mixin_mixout/mixin_mixout_hifi5.c
	)
endif()

zephyr_library_sources_ifdef(CONFIG_COMP_TONE
	${SOF_AUDIO_PATH}/tone.c
)

if(CONFIG_ZEPHYR_NATIVE_DRIVERS)
	zephyr_library_sources_ifdef(CONFIG_COMP_DAI
		${SOF_AUDIO_PATH}/dai-zephyr.c
)
else()
	zephyr_library_sources_ifdef(CONFIG_COMP_DAI
		${SOF_AUDIO_PATH}/dai-legacy.c
)
endif()

zephyr_library_sources_ifdef(CONFIG_IPC4_GATEWAY
	${SOF_AUDIO_PATH}/copier/copier_ipcgtw.c
)

zephyr_library_sources_ifdef(CONFIG_SAMPLE_KEYPHRASE
	${SOF_SAMPLES_PATH}/audio/detect_test.c
)

zephyr_library_sources_ifdef(CONFIG_COMP_VOLUME
	${SOF_AUDIO_PATH}/volume/volume_hifi4.c
	${SOF_AUDIO_PATH}/volume/volume_hifi3.c
	${SOF_AUDIO_PATH}/volume/volume_generic.c
	${SOF_AUDIO_PATH}/volume/volume_hifi4_with_peakvol.c
	${SOF_AUDIO_PATH}/volume/volume_hifi3_with_peakvol.c
	${SOF_AUDIO_PATH}/volume/volume_generic_with_peakvol.c
	${SOF_AUDIO_PATH}/volume/volume.c
	${SOF_AUDIO_PATH}/volume/volume_${ipc_suffix}.c
)

zephyr_library_sources_ifdef(CONFIG_COMP_MODULE_ADAPTER
	${SOF_AUDIO_PATH}/module_adapter/module_adapter.c
	${SOF_AUDIO_PATH}/module_adapter/module_adapter_${ipc_suffix}.c
	${SOF_AUDIO_PATH}/module_adapter/module/generic.c
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

zephyr_include_directories_ifdef(CONFIG_INTEL_MODULES
	${SOF_SRC_PATH}/include/sof/audio/module_adapter/iadk/
	${SOF_SRC_PATH}/include/sof/audio/module_adapter/library/
)

zephyr_library_sources_ifdef(CONFIG_INTEL_MODULES
	${SOF_AUDIO_PATH}/module_adapter/module/modules.c
	${SOF_AUDIO_PATH}/module_adapter/iadk/module_initial_settings_concrete.cpp
	${SOF_AUDIO_PATH}/module_adapter/iadk/iadk_module_adapter.cpp
	${SOF_AUDIO_PATH}/module_adapter/iadk/system_agent.cpp
	${SOF_AUDIO_PATH}/module_adapter/library/native_system_agent.c
	${SOF_AUDIO_PATH}/module_adapter/library/native_system_service.c
)

if (CONFIG_COMP_MODULE_ADAPTER)
zephyr_library_sources_ifdef(CONFIG_CADENCE_CODEC
	${SOF_AUDIO_PATH}/module_adapter/module/cadence.c
)

if (CONFIG_CADENCE_CODEC_MP3_DEC)
zephyr_library_import(xa_mp3_dec ${CONFIG_CADENCE_CODEC_MP3_DEC_LIB})
endif()

if (CONFIG_CADENCE_CODEC_MP3_ENC)
zephyr_library_import(xa_mp3_enc ${CONFIG_CADENCE_CODEC_MP3_ENC_LIB})
endif()

if (CONFIG_CADENCE_CODEC_AAC_DEC)
zephyr_library_import(xa_aac_dec ${CONFIG_CADENCE_CODEC_AAC_DEC_LIB})
endif()

zephyr_library_sources_ifdef(CONFIG_PASSTHROUGH_CODEC
	${SOF_AUDIO_MODULES_PATH}/passthrough.c
)
endif()

zephyr_library_sources_ifdef(CONFIG_COMP_CHAIN_DMA
	${SOF_AUDIO_PATH}/chain_dma.c
)

zephyr_library_sources_ifdef(CONFIG_COMP_SRC
	${SOF_AUDIO_PATH}/src/src_hifi2ep.c
	${SOF_AUDIO_PATH}/src/src_generic.c
	${SOF_AUDIO_PATH}/src/src_hifi3.c
	${SOF_AUDIO_PATH}/src/src_hifi4.c
	${SOF_AUDIO_PATH}/src/src.c
	${SOF_AUDIO_PATH}/src/src_${ipc_suffix}.c
)

zephyr_library_sources_ifdef(CONFIG_COMP_SRC_LITE
	${SOF_AUDIO_PATH}/src/src_lite.c
)
zephyr_library_sources_ifdef(CONFIG_COMP_BASEFW_IPC4
	${SOF_AUDIO_PATH}/base_fw.c
)

zephyr_library_sources_ifdef(CONFIG_IPC4_BASE_FW_INTEL
	${SOF_AUDIO_PATH}/base_fw_intel.c
)

zephyr_library_sources_ifdef(CONFIG_COMP_COPIER
	${SOF_AUDIO_PATH}/copier/copier_generic.c
	${SOF_AUDIO_PATH}/copier/copier_hifi.c
	${SOF_AUDIO_PATH}/copier/copier.c
	${SOF_AUDIO_PATH}/copier/copier_host.c
	${SOF_AUDIO_PATH}/copier/copier_dai.c
)

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

zephyr_library_sources_ifdef(CONFIG_COMP_ARIA
	${SOF_AUDIO_PATH}/aria/aria.c
	${SOF_AUDIO_PATH}/aria/aria_hifi5.c
	${SOF_AUDIO_PATH}/aria/aria_hifi3.c
	${SOF_AUDIO_PATH}/aria/aria_generic.c
)

zephyr_library_sources_ifdef(CONFIG_COMP_CROSSOVER
	${SOF_AUDIO_PATH}/crossover/crossover.c
	${SOF_AUDIO_PATH}/crossover/crossover_generic.c
)

zephyr_library_sources_ifdef(CONFIG_COMP_CROSSOVER
	${SOF_AUDIO_PATH}/crossover/crossover_${ipc_suffix}.c
)

if(CONFIG_COMP_DRC STREQUAL "m")
	add_subdirectory(${SOF_AUDIO_PATH}/drc/llext
			 ${PROJECT_BINARY_DIR}/drc_llext)
	add_dependencies(app drc)
elseif(CONFIG_COMP_DRC)
	zephyr_library_sources(
		${SOF_AUDIO_PATH}/drc/drc.c
		${SOF_AUDIO_PATH}/drc/drc_generic.c
		${SOF_AUDIO_PATH}/drc/drc_math_generic.c
		${SOF_AUDIO_PATH}/drc/drc_hifi3.c
		${SOF_AUDIO_PATH}/drc/drc_hifi4.c
		${SOF_AUDIO_PATH}/drc/drc_math_hifi3.c
	)
endif()

zephyr_library_sources_ifdef(CONFIG_COMP_MULTIBAND_DRC
	${SOF_AUDIO_PATH}/multiband_drc/multiband_drc.c
	${SOF_AUDIO_PATH}/multiband_drc/multiband_drc_generic.c
)

zephyr_library_sources_ifdef(CONFIG_COMP_MULTIBAND_DRC
	${SOF_AUDIO_PATH}/multiband_drc/multiband_drc_${ipc_suffix}.c
)

zephyr_library_sources_ifdef(CONFIG_COMP_GOOGLE_RTC_AUDIO_PROCESSING
	${SOF_AUDIO_PATH}/google/google_rtc_audio_processing.c
)

zephyr_library_sources_ifdef(CONFIG_GOOGLE_RTC_AUDIO_PROCESSING_MOCK
	${SOF_AUDIO_PATH}/google/google_rtc_audio_processing_mock.c
)

if(CONFIG_COMP_GOOGLE_RTC_AUDIO_PROCESSING AND NOT CONFIG_GOOGLE_RTC_AUDIO_PROCESSING_MOCK)
	zephyr_include_directories(../third_party/include)
	target_link_directories(SOF INTERFACE ../third_party/lib)
	target_link_libraries(SOF INTERFACE google_rtc_audio_processing)
	target_link_libraries(SOF INTERFACE c++)
	target_link_libraries(SOF INTERFACE c++abi)
	target_link_libraries(SOF INTERFACE m)
	target_link_libraries(SOF INTERFACE c)
	target_link_libraries(SOF INTERFACE gcc)
endif()

zephyr_library_sources_ifdef(CONFIG_COMP_IGO_NR
	${SOF_AUDIO_PATH}/igo_nr/igo_nr.c
)

zephyr_library_sources_ifdef(CONFIG_COMP_IGO_NR_STUB
	${SOF_AUDIO_PATH}/igo_nr/igo_nr_stub.c
)

zephyr_library_sources_ifdef(CONFIG_COMP_RTNR
	${SOF_AUDIO_PATH}/rtnr/rtnr.c
)

if(CONFIG_SAMPLE_SMART_AMP STREQUAL "m")
	add_subdirectory(${SOF_SAMPLES_PATH}/audio/smart_amp_test_llext
			 ${PROJECT_BINARY_DIR}/smart_amp_test_llext)
	add_dependencies(app smart_amp_test)
elseif(CONFIG_SAMPLE_SMART_AMP)
	zephyr_library_sources(
		${SOF_SAMPLES_PATH}/audio/smart_amp_test_${ipc_suffix}.c
	)
endif()

zephyr_library_sources_ifdef(CONFIG_COMP_RTNR_STUB
	${SOF_AUDIO_PATH}/rtnr/rtnr_stub.c
)

zephyr_library_sources_ifdef(CONFIG_COMP_TDFB
	${SOF_AUDIO_PATH}/tdfb/tdfb.c
	${SOF_AUDIO_PATH}/tdfb/tdfb_direction.c
	${SOF_AUDIO_PATH}/tdfb/tdfb_generic.c
	${SOF_AUDIO_PATH}/tdfb/tdfb_hifiep.c
	${SOF_AUDIO_PATH}/tdfb/tdfb_hifi3.c
	${SOF_AUDIO_PATH}/tdfb/tdfb_${ipc_suffix}.c
)

zephyr_library_sources_ifdef(CONFIG_SQRT_FIXED
	${SOF_MATH_PATH}/sqrt_int16.c
)

zephyr_library_sources_ifdef(CONFIG_MATH_EXP
	${SOF_MATH_PATH}/exp_fcn.c
	${SOF_MATH_PATH}/exp_fcn_hifi.c
)

zephyr_library_sources_ifdef(CONFIG_COMP_UP_DOWN_MIXER
	${SOF_AUDIO_PATH}/up_down_mixer/up_down_mixer.c
	${SOF_AUDIO_PATH}/up_down_mixer/up_down_mixer_hifi3.c
)

zephyr_library_sources_ifdef(CONFIG_COMP_MUX
	${SOF_AUDIO_PATH}/mux/mux.c
	${SOF_AUDIO_PATH}/mux/mux_generic.c
)

zephyr_library_sources_ifdef(CONFIG_COMP_MUX
	${SOF_AUDIO_PATH}/mux/mux_${ipc_suffix}.c
)

zephyr_library_sources_ifdef(CONFIG_COMP_GOOGLE_HOTWORD_DETECT
	${SOF_AUDIO_PATH}/google/google_hotword_detect.c
)

zephyr_library_sources_ifdef(CONFIG_DTS_CODEC
	${SOF_AUDIO_PATH}/codec/dts/dts.c
)
if (CONFIG_DTS_CODEC)
	if (CONFIG_DTS_CODEC_STUB)
		zephyr_library_sources(
			${SOF_AUDIO_PATH}/codec/dts/dts_stub.c
		)
	else()
		zephyr_library_import(DtsCodec
			${sof_top_dir}/third_party/lib/libdts-sof-interface-i32.a)
	endif()
endif()

zephyr_library_sources_ifdef(CONFIG_WAVES_CODEC
	${SOF_AUDIO_PATH}/module_adapter/module/waves/waves.c
)

zephyr_library_sources_ifdef(CONFIG_WAVES_CODEC_STUB
	${SOF_AUDIO_PATH}/module_adapter/module/waves/maxx_stub.c
)

zephyr_library_sources_ifdef(CONFIG_PROBE
	${SOF_SRC_PATH}/probe/probe.c
)

zephyr_library_sources_ifdef(CONFIG_MULTICORE
	${SOF_SRC_PATH}/idc/idc.c
)

zephyr_library_sources_ifdef(CONFIG_HAVE_AGENT
	${SOF_LIB_PATH}/agent.c
)

zephyr_library_sources_ifdef(CONFIG_AMS
	${SOF_LIB_PATH}/ams.c
)

zephyr_library_sources_ifdef(CONFIG_GDB_DEBUG
	${SOF_DEBUG_PATH}/gdb/gdb.c
	${SOF_DEBUG_PATH}/gdb/ringbuffer.c
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
