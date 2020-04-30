# This is still WIP - Not fully validated on any platform.

if(CONFIG_SOF)

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

# Initial SOF module will contain
#
# 1. Application logic - pipeline, audio components, IPC processing, topology
# 2. IP drivers - SSP, DMIC, PM, IPC will transition to Zephyr directly over
#                 time and be removed from the SOF repo.
# 3. Platform IP - PM, init, clocks, IRQs will transition directly to Zephyr
#                  over time and be removed from SOF repo.
# 4. RTOS logic - scheduler, allocator, notifier - as with 2 & 3.
zephyr_interface_library_named(SOF)

# default SOF includes
target_include_directories(SOF INTERFACE ../zephyr/include)
target_include_directories(SOF INTERFACE ../src/include)
target_include_directories(SOF INTERFACE ../src/arch/xtensa/include)

# SOF module init
zephyr_library_named(modules_sof)
zephyr_include_directories(
	include
)

# SOC level sources
# Files that are commented may not be needed.

# BYT, CHT, BSW
if (CONFIG_SOC_SERIES_INTEL_BAYTRAIL)
	zephyr_library_sources(
		../src/drivers/intel/baytrail/ipc.c
		#../src/drivers/intel/baytrail/interrupt.c
		../src/drivers/intel/baytrail/timer.c
		../src/drivers/intel/baytrail/ssp.c
		 ../src/drivers/intel/pmc-ipc.c
	)

	target_include_directories(SOF INTERFACE ../src/platform/baytrail/include)
endif()

# HSW, BDW
if (CONFIG_SOC_SERIES_INTEL_BROADWELL)
	zephyr_library_sources(
		../src/drivers/intel/haswell/ipc.c
		#../src/drivers/intel/haswell/interrupt.c
		../src/drivers/intel/haswell/timer.c
		../src/drivers/intel/haswell/ssp.c
	)

	target_include_directories(SOF INTERFACE ../src/platform/haswell/include)
endif()

# APL, KBL, SKL
if (CONFIG_SOC_SERIES_INTEL_CAVS_V15)
	zephyr_library_sources(
		../src/drivers/intel/cavs/hda-dma.c
		../src/drivers/intel/cavs/hda.c
		../src/drivers/intel/cavs/ipc.c
		../src/drivers/intel/cavs/dmic.c
		#../src/drivers/intel/cavs/interrupt.c
		#../src/drivers/intel/cavs/idc.c
		../src/drivers/intel/cavs/timer.c
		../src/drivers/intel/cavs/ssp.c
		../src/drivers/intel/cavs/mn.c
	)

	# CAVS15 include seems to be set in Zephyr. TODO also set common dir.
	target_include_directories(SOF INTERFACE ../zephyr/include/cavs15)
	zephyr_include_directories(../../../../zephyr/soc/xtensa/intel_adsp/common/include)
endif()

# CNL
if (CONFIG_SOC_SERIES_INTEL_CAVS_V18)
	zephyr_library_sources(
		../src/drivers/intel/cavs/hda-dma.c
		../src/drivers/intel/cavs/hda.c
		../src/drivers/intel/cavs/ipc.c
		#../src/drivers/intel/cavs/sue-ipc.c
		../src/drivers/intel/cavs/dmic.c
		# ../src/drivers/intel/cavs/interrupt.c
		../src/drivers/intel/cavs/idc.c
		../src/drivers/intel/cavs/timer.c
		#../src/drivers/intel/cavs/sue-iomux.c
		../src/drivers/intel/cavs/ssp.c
		../src/drivers/intel/cavs/mn.c
		../src/drivers/intel/cavs/alh.c
	)

	target_include_directories(SOF INTERFACE ../zephyr/include/cavs18)
	zephyr_include_directories(../../../../zephyr/soc/xtensa/intel_adsp/common/include)

endif()

# ICL
if (CONFIG_SOC_SERIES_INTEL_CAVS_V20)
	zephyr_library_sources(
		../src/drivers/intel/cavs/hda-dma.c
		../src/drivers/intel/cavs/hda.c
		../src/drivers/intel/cavs/ipc.c
		../src/drivers/intel/cavs/dmic.c
		#../src/drivers/intel/cavs/interrupt.c
		../src/drivers/intel/cavs/idc.c
		../src/drivers/intel/cavs/timer.c
		../src/drivers/intel/cavs/ssp.c
		../src/drivers/intel/cavs/mn.c
		../src/drivers/intel/cavs/alh.c
	)

	target_include_directories(SOF INTERFACE ../zephyr/include/cavs20)
	zephyr_include_directories(../../../../zephyr/soc/xtensa/intel_adsp/common/include)
endif()

# TGL
if (CONFIG_SOC_SERIES_INTEL_CAVS_V25)
	zephyr_library_sources(
		../src/drivers/intel/cavs/hda-dma.c
		../src/drivers/intel/cavs/hda.c
		../src/drivers/intel/cavs/ipc.c
		../src/drivers/intel/cavs/dmic.c
		#../src/drivers/intel/cavs/interrupt.c
		../src/drivers/intel/cavs/idc.c
		../src/drivers/intel/cavs/timer.c
		../src/drivers/intel/cavs/ssp.c
		../src/drivers/intel/cavs/mn.c
		../src/drivers/intel/cavs/alh.c
	)

	target_include_directories(SOF INTERFACE ../zephyr/include/cavs25)
	zephyr_include_directories(../../../../zephyr/soc/xtensa/intel_adsp/common/include)
endif()

# IMX8
if (CONFIG_SOC_SERIES_NXP_IMX8)
	zephyr_library_sources(
		#../src/drivers/imx/sdma.c
		#../src/drivers/imx/edma.c
		#../src/drivers/imx/sai.c
		#../src/drivers/imx/ipc.c
		#../src/drivers/imx/esai.c
		#../src/drivers/imx/interrupt.c
		#../src/drivers/imx/timer.c
	)
endif()

# Files used on all platforms.
# Commented files will be added/removed as integration dictates.
zephyr_library_sources(
	../src/trace/dma-trace.c
	#../src/trace/trace.c
	#../src/drivers/dw/ssi-spi.c
	#../src/drivers/dw/gpio.c
	../src/drivers/dw/dma.c
	#../src/drivers/interrupt.c
	../src/drivers/generic/dummy-dma.c
	../src/ipc/dma-copy.c
	../src/ipc/ipc.c
	../src/ipc/handler.c
	../src/ipc/probe_support.c
	../src/ipc/user_abi_version.c
	../src/ipc/ipc-host-ptable.c
	../src/ipc/cc_version.c
	../src/debug/gdb/gdb.c
	../src/debug/gdb/ringbuffer.c
	../src/debug/panic.c
	../src/spinlock.c
	../src/math/decibels.c
	../src/math/numbers.c
	../src/math/trig.c
	#../src/init/init.c
	#../src/schedule/task.c
	#../src/schedule/timer_domain.c
	#../src/schedule/schedule.c
	#../src/schedule/edf_schedule.c
	#../src/schedule/dma_single_chan_domain.c
	#../src/schedule/dma_multi_chan_domain.c
	#../src/schedule/ll_schedule.c
	../src/lib/clk.c
	../src/lib/notifier.c
	../src/lib/lib.c
	../src/lib/pm_runtime.c
	../src/lib/agent.c
	#../src/lib/alloc.c
	../src/lib/wait.c
	../src/lib/dma.c
	../src/lib/dai.c
	../src/audio/kpb.c
	../src/audio/pipeline_static.c
	../src/audio/selector/selector_generic.c
	../src/audio/selector/selector.c
	../src/audio/channel_map.c
	../src/audio/switch.c
	../src/audio/src/src_hifi2ep.c
	../src/audio/src/src_generic.c
	../src/audio/src/src_hifi3.c
	../src/audio/src/src.c
	../src/audio/pcm_converter/pcm_converter_hifi3.c
	../src/audio/pcm_converter/pcm_converter_generic.c
	../src/audio/volume/volume_hifi3.c
	../src/audio/volume/volume_generic.c
	../src/audio/volume/volume.c
	../src/audio/mixer.c
	../src/audio/tone.c
	../src/audio/eq_fir/fir_hifi3.c
	../src/audio/eq_fir/fir_hifi2ep.c
	../src/audio/eq_fir/eq_fir.c
	../src/audio/eq_fir/fir.c
	../src/audio/detect_test.c
	../src/audio/host.c
	../src/audio/asrc/asrc.c
	../src/audio/asrc/asrc_farrow_hifi3.c
	../src/audio/asrc/asrc_farrow.c
	../src/audio/asrc/asrc_farrow_generic.c
	../src/audio/dcblock/dcblock_generic.c
	../src/audio/dcblock/dcblock.c
	../src/audio/mux/mux.c
	../src/audio/mux/mux_generic.c
	../src/audio/eq_iir/iir_generic.c
	../src/audio/eq_iir/iir_hifi3.c
	../src/audio/eq_iir/iir.c
	../src/audio/eq_iir/eq_iir.c
	../src/audio/buffer.c
	../src/audio/component.c
	../src/audio/dai.c
	../src/audio/pipeline.c
	#../src/probe/probe.c
	wrapper.c
	edf_schedule.c
	ll_schedule.c
	schedule.c
)

zephyr_library_link_libraries(SOF)
target_link_libraries(SOF INTERFACE zephyr_interface)

# Setup SOF directories
set(SOF_ROOT_SOURCE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
set(SOF_ROOT_BINARY_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set(GENERATED_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
file(MAKE_DIRECTORY ${GENERATED_DIRECTORY}/include)

# generated files
set(VERSION_H_PATH ${GENERATED_DIRECTORY}/include/version.h)
set(DOT_CONFIG_PATH ${GENERATED_DIRECTORY}/.config)
set(CONFIG_H_PATH ${GENERATED_DIRECTORY}/include/config.h)

# create version.h
include(../scripts/cmake/version.cmake)

# SOF uses GNU C99 extensions. TODO other flags required ?
target_compile_options(SOF INTERFACE -std=gnu99 -fno-inline-functions)

# Toolchain info
add_definitions(-DXCC_TOOLS_VERSION="${ZEPHYR_TOOLCHAIN_VARIANT}" -DCC_OPTIMIZE_FLAGS="${OPTIMIZATION_FLAG}")

# Create Trace realtive file paths
sof_append_relative_path_definitions(modules_sof)
endif()

