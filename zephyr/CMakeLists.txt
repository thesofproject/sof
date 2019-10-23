if(CONFIG_SOF)
zephyr_interface_library_named(SOF)

# Includes needed by headers under SoC's include/platform
zephyr_include_directories(../src/platform/intel/cavs/include)

# include redefines incompatible things on src/include
target_include_directories(SOF INTERFACE
	include
	#../src/arch/host/include
	../src/platform/intel/cavs/include
	../src/include)

if(CONFIG_SMP)
target_include_directories(SOF INTERFACE ../src/arch/xtensa/smp/include)
else()
target_include_directories(SOF INTERFACE ../src/arch/xtensa/up/include)
endif()

zephyr_library()
zephyr_library_compile_definitions(CONFIG_LIBRARY=1)
zephyr_library_sources(
	# Dummy version, might move these to Zephyr
	alloc.c
	dma-trace.c
	ll_schedule.c
	edf_schedule.c
	schedule.c
	panic.c
	pm.c
	ipc.c
	interrupt.c
	# Modified drivers from SOF
	drivers/intel/cavs/interrupt.c
	# SOF src/audio
	../src/audio/host.c
	../src/audio/pipeline.c
	#../src/audio/pipeline_static.c
	../src/audio/component.c
	../src/audio/buffer.c
	../src/audio/dai.c
	# SOF src/ipc
	../src/ipc/ipc.c
	../src/ipc/handler.c
	# SOF src/lib
	../src/lib/lib.c
	../src/lib/dai.c
	../src/lib/dma.c
	#../src/lib/alloc.c
	#../src/lib/ll_schedule.c
	#../src/lib/notifier.c
	#../src/lib/edf_schedule.c
	#../src/lib/schedule.c
	#../src/lib/agent.c
	#../src/lib/interrupt.c
	#../src/lib/pm_runtime.c
	../src/lib/clk.c
	#../src/lib/dma.c
	#../src/lib/dai.c
	#../src/lib/panic.c
	#../src/lib/wait.c
	# SOF src/math
	../src/math/numbers.c
	../src/math/trig.c
	# SOF src/drivers
	../src/drivers/dw/dma.c
	../src/drivers/intel/cavs/hda.c
	../src/drivers/intel/cavs/hda-dma.c
	../src/drivers/intel/cavs/ssp.c
	# Platform code
	#../src/platform/intel/cavs/pm_runtime.c
	../src/drivers/intel/cavs/timer.c
	# SOF CAVS platform code
	../src/platform/intel/cavs/dai.c
	../src/platform/intel/cavs/dma.c
)

zephyr_library_sources_ifdef(CONFIG_SOF_STATIC_PIPELINE pipeline_static.c)
zephyr_library_sources_ifdef(CONFIG_SOF_COMP_DAI ../src/audio/dai.c)
zephyr_library_sources_ifdef(CONFIG_SOF_COMP_TONE ../src/audio/tone.c)
zephyr_library_sources_ifdef(CONFIG_SOF_COMP_MIXER ../src/audio/mixer.c)
zephyr_library_sources_ifdef(CONFIG_SOF_COMP_MUX ../src/audio/mux/mux.c ../src/audio/mux/mux_generic.c)
zephyr_library_sources_ifdef(CONFIG_SOF_COMP_SWITCH ../src/audio/switch.c)
#zephyr_library_sources_ifdef(CONFIG_SOF_COMP_KPB ../src/audio/kpb.c)
zephyr_library_sources_ifdef(CONFIG_SOF_COMP_TEST_KEYPHRASE
	../src/audio/detect_test.c)
zephyr_library_sources_ifdef(CONFIG_SOF_COMP_VOLUME
	../src/audio/volume/volume_generic.c
	../src/audio/volume/volume_hifi3.c
	../src/audio/volume/volume.c)
zephyr_library_sources_ifdef(CONFIG_SOF_COMP_SRC
	../src/audio/src/src_generic.c
	../src/audio/src/src_hifi2ep.c
	../src/audio/src/src_hifi3.c
	../src/audio/src/src.c)
zephyr_library_sources_ifdef(CONFIG_SOF_COMP_FIR
	../src/audio/eq_fir/eq_fir.c
	../src/audio/eq_fir/fir_hifi2ep.c
	../src/audio/eq_fir/fir_hifi3.c
	../src/audio/eq_fir/fir.c)
zephyr_library_sources_ifdef(CONFIG_SOF_COMP_IIR
	../src/audio/eq_iir/eq_iir.c
	../src/audio/eq_iir/iir.c)
zephyr_library_sources_ifdef(CONFIG_SOF_COMP_SEL
	../src/audio/selector/selector_generic.c
	../src/audio/selector/selector.c)
zephyr_library_sources_ifdef(CONFIG_SOF_USE_DMIC
	../src/drivers/intel/cavs/dmic.c
	)

zephyr_library_link_libraries(SOF)
target_link_libraries(SOF INTERFACE zephyr_interface)

add_library(sof_base_module
	../src/platform/apollolake/base_module.c
	)
target_include_directories(sof_base_module PUBLIC
	../src/include
	${SOC_DIR}/${ARCH}/${SOC_PATH}/include
	)

add_library(sof_boot_module
	../src/platform/apollolake/boot_module.c
	)
target_include_directories(sof_boot_module PUBLIC
	../src/include
	${SOC_DIR}/${ARCH}/${SOC_PATH}/include
	)

set(sof_module ${PROJECT_SOURCE_DIR}/../modules/audio/sof)
set(sof_rimage_keys ${PROJECT_SOURCE_DIR}/../modules/audio/sof/rimage/keys)
set(sof_rimage_version ${sof_module}/zephyr/include/version.h)

include(ExternalProject)
ExternalProject_Add(rimage
	DOWNLOAD_COMMAND ""
	SOURCE_DIR "${sof_module}/rimage"
	PREFIX "${PROJECT_BINARY_DIR}/rimage_ep"
	BINARY_DIR "${PROJECT_BINARY_DIR}/rimage_ep/build"
	BUILD_ALWAYS 1
	CMAKE_ARGS -DPEM_KEY_PREFIX=${sof_rimage_keys} -DVERSION_H_PATH=${sof_rimage_version}
	INSTALL_COMMAND ""
	)

#
# Bootloader build
#

add_executable(bootloader
	../src/arch/xtensa/up/xtos/_vectors.S
	../src/arch/xtensa/boot_entry.S
	../src/arch/xtensa/smp/xtos/memctl_default.S
	../src/arch/xtensa/up/xtos/reset-vector.S
	#../src/arch/xtensa/up/hal/cache_asm.S
	../src/arch/xtensa/boot_loader.c
	)
target_include_directories(bootloader PUBLIC
	../src/include
	../src/arch/xtensa/include
	../src/platform/apollolake/include/arch
	${SOC_DIR}/${ARCH}/${SOC_PATH}/include
	)

set(zephyr_sdk $ENV{ZEPHYR_SDK_INSTALL_DIR})

set_source_files_properties(../src/arch/xtensa/up/xtos/_vectors.S PROPERTIES COMPILE_FLAGS -DASSEMBLY)
set_source_files_properties(../src/arch/xtensa/boot_entry.S PROPERTIES COMPILE_FLAGS -DASSEMBLY)
set_source_files_properties(../src/arch/xtensa/up/xtos/reset-vector.S PROPERTIES COMPILE_FLAGS -DASSEMBLY)

target_compile_options(bootloader PUBLIC -fno-inline-functions -mlongcalls -mtext-section-literals -DCONFIG_BOOT_LOADER -DCONFIG_APOLLOLAKE -DCONFIG_BUILD_VM_ROM)

target_link_libraries(bootloader PUBLIC -Wl,--no-check-sections -ucall_user_start -Wl,-static -nostdlib)
target_link_libraries(bootloader PRIVATE -lhal -L${zephyr_sdk}/xtensa/intel_apl_adsp/xtensa-zephyr-elf/lib)
target_link_libraries(bootloader PRIVATE -T${sof_module}/zephyr/boot_ldr.x)

endif()
