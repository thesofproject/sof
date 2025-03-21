# SPDX-License-Identifier: BSD-3-Clause

set(base_files
	channel_map.c
	component.c
	source_api_helper.c
	sink_api_helper.c
	sink_source_utils.c
	audio_stream.c
	channel_map.c
)

### Common actions for Zephyr/XTOS ###
if(NOT CONFIG_COMP_MODULE_SHARED_LIBRARY_BUILD)
	# directories included unconditionally (alphabetical order)
	add_subdirectory(buffers)
	add_subdirectory(pcm_converter)
	add_subdirectory(pipeline)

	# directories and files included conditionally (alphabetical order)
	if(CONFIG_COMP_ARIA)
		add_subdirectory(aria)
	endif()
	if(CONFIG_COMP_ASRC)
		add_subdirectory(asrc)
	endif()
	if(CONFIG_COMP_BASEFW_IPC4 AND NOT CONFIG_LIBRARY)
		add_local_sources(sof base_fw.c)
	endif()
	add_local_sources_ifdef(CONFIG_IPC4_BASE_FW_INTEL sof base_fw_intel.c)
	if(CONFIG_COMP_CHAIN_DMA)
		add_local_sources(sof chain_dma.c)
	endif()
	if(CONFIG_COMP_COPIER)
		add_subdirectory(copier)
	endif()
	if(CONFIG_COMP_CROSSOVER)
		add_subdirectory(crossover)
	endif()
	if(CONFIG_COMP_DCBLOCK)
		add_subdirectory(dcblock)
	endif()
	if(CONFIG_COMP_DRC)
		add_subdirectory(drc)
	endif()
	if(CONFIG_COMP_FIR)
		add_subdirectory(eq_fir)
	endif()
	if(CONFIG_COMP_IIR)
		add_subdirectory(eq_iir)
	endif()
	if(CONFIG_COMP_KPB AND NOT CONFIG_LIBRARY_STATIC)
		add_local_sources(sof
			kpb.c
		)
	endif()
	if(CONFIG_COMP_MFCC)
		add_subdirectory(mfcc)
	endif()
	if(CONFIG_COMP_MIXER)
                add_subdirectory(mixer)
	endif()
	if(CONFIG_COMP_MIXIN_MIXOUT)
                add_subdirectory(mixin_mixout)
	endif()
	if(CONFIG_COMP_MODULE_ADAPTER)
		add_subdirectory(module_adapter)
	endif()
	if(CONFIG_COMP_MULTIBAND_DRC)
		add_subdirectory(multiband_drc)
	endif()
	if(CONFIG_COMP_MUX)
		add_subdirectory(mux)
	endif()
	if(CONFIG_COMP_SEL)
		add_subdirectory(selector)
	endif()
	if(CONFIG_COMP_SRC)
		add_subdirectory(src)
	endif()
	if(CONFIG_COMP_TDFB)
		add_subdirectory(tdfb)
	endif()
	if(CONFIG_COMP_TONE)
		add_local_sources(sof
			tone.c
		)
	endif()
	if(CONFIG_COMP_UP_DOWN_MIXER)
		add_subdirectory(up_down_mixer)
	endif()
	if(CONFIG_COMP_VOLUME)
		add_subdirectory(volume)
	endif()
	if(CONFIG_DTS_CODEC)
		add_subdirectory(codec)
	endif()
	if(CONFIG_ZEPHYR_NATIVE_DRIVERS)
		list(APPEND base_files host-zephyr.c)
		sof_list_append_ifdef(CONFIG_COMP_DAI base_files dai-zephyr.c)
	else()
		list(APPEND base_files host-legacy.c)
		sof_list_append_ifdef(CONFIG_COMP_DAI base_files dai-legacy.c)
	endif()
endif()

### Common files (also used in shared library build)

sof_list_append_ifdef(CONFIG_COMP_BLOB base_files data_blob.c)

### (end of common files)

is_zephyr(it_is)
if(it_is) ###  Zephyr ###

	zephyr_library_sources(
		${base_files}
	)

	return()
endif()

### XTOS normal build (not shared library) ###

if(NOT CONFIG_COMP_MODULE_SHARED_LIBRARY_BUILD)
	add_local_sources(sof
		host-legacy.c
		${base_files}
	)

	if(CONFIG_COMP_DAI)
		add_local_sources(sof
			dai-legacy.c
		)
	endif()
	if(CONFIG_COMP_SMART_AMP)
		add_subdirectory(smart_amp)
	endif()
	if(CONFIG_COMP_IGO_NR)
		add_subdirectory(igo_nr)
	endif()
	if(CONFIG_COMP_RTNR)
		add_subdirectory(rtnr)
	endif()
	if(CONFIG_INTEL_ADSP_MIC_PRIVACY)
		add_subdirectory(mic_privacy_manager)
	endif()
	add_subdirectory(google)

	return()
endif()

### Shared library build (CONFIG_COMP_MODULE_SHARED_LIBRARY_BUILD) ###

subdirs(pipeline)

if(CONFIG_COMP_MODULE_ADAPTER)
		add_subdirectory(module_adapter)
endif()

add_local_sources(sof
	data_blob.c
	buffers/comp_buffer.c
	buffers/audio_buffer.c
	${base_files}
)

# Audio Modules with various optimizaitons

# add rules for module compilation and installation
function(sof_audio_add_module lib_name compile_flags)
	add_library(${lib_name} MODULE "")
	target_link_libraries(${lib_name} PRIVATE sof_options)
	target_link_libraries(${lib_name} PRIVATE -Wl,--export-dynamic)
	target_compile_options(${lib_name} PRIVATE ${compile_flags})
	add_local_sources(${lib_name} ${ARGN})
	sof_append_relative_path_definitions(${lib_name})
	install(TARGETS ${lib_name} DESTINATION lib)
endfunction()

include(CheckCCompilerFlag)

set(available_optimizations)

# checks if flag is supported by compiler and sets needed flags
# note: to debug vectorisation please add "-fopt-info-vec-note" option after
# the enable command below.
macro(check_optimization opt_name flag enable_cmd extra_define)
	check_c_compiler_flag(${flag} compiles_flag_${opt_name})
	if(compiles_flag_${opt_name})
		list(APPEND available_optimizations ${opt_name})
		set(${opt_name}_flags ${flag} ${extra_define} ${enable_cmd} -ffast-math)
	endif()
endmacro()

# modules will be compiled only for flags supported by compiler
check_optimization(sse42 -msse4.2 -ftree-vectorize -DOPS_SSE42)
check_optimization(avx -mavx -ftree-vectorize -DOPS_AVX)
check_optimization(avx2 -mavx2 -ftree-vectorize -DOPS_AVX2)
check_optimization(fma -mfma -ftree-vectorize -DOPS_FMA)
check_optimization(hifi2ep -mhifi2ep "" -DOPS_HIFI2EP)
check_optimization(hifi3 -mhifi3 "" -DOPS_HIFI3)

set(sof_audio_modules mixer volume src asrc eq-fir eq-iir dcblock crossover tdfb drc multiband_drc mfcc mux)

# sources for each module
if(CONFIG_IPC_MAJOR_3)
	set(volume_sources volume/volume.c volume/volume_generic.c volume/volume_ipc3.c)
	set(asrc_sources asrc/asrc_ipc3.c)
	set(src_sources src/src.c src/src_ipc3.c src/src_generic.c)
	set(eq-iir_sources eq_iir/eq_iir_ipc3.c eq_iir/eq_iir_generic.c)
	set(eq-fir_sources eq_fir/eq_fir_ipc3.c)
	set(tdfb_sources tdfb/tdfb_ipc3.c)
	set(tdfb_sources multiband_drc/multiband_drc_ipc3.c)
	set(dcblock_sources dcblock/dcblock_ipc3.c)
	set(mux_sources mux/mux_ipc3.c)
	set(crossover_sources crossover/crossover_ipc3.c)
elseif(CONFIG_IPC_MAJOR_4)
	set(volume_sources volume/volume.c volume/volume_generic.c volume/volume_ipc4.c)
	set(asrc_sources asrc/asrc_ipc4.c)
	set(src_sources src/src.c src/src_ipc4.c src/src_generic.c)
	set(eq-iir_sources eq_iir/eq_iir_ipc4.c eq_iir/eq_iir_generic.c)
	set(eq-fir_sources eq_fir/eq_fir_ipc4.c)
	set(tdfb_sources tdfb/tdfb_ipc4.c)
	set(tdfb_sources multiband_drc/multiband_drc_ipc4.c)
	set(dcblock_sources dcblock/dcblock_ipc4.c)
	set(mux_sources mux/mux_ipc4.c)
	set(crossover_sources crossover/crossover_ipc4.c)
endif()

if(CONFIG_IPC_MAJOR_3)
	set(mixer_src mixer/mixer.c mixer/mixer_generic.c mixer/mixer_hifi3.c)
elseif(CONFIG_IPC_MAJOR_4)
	set(mixer_src mixin_mixout/mixin_mixout.c mixin_mixout/mixin_mixout_generic.c mixin_mixout/mixin_mixout_hifi3.c)
endif()

set(mixer_sources ${mixer_src})
set(asrc_sources asrc/asrc.c asrc/asrc_farrow.c asrc/asrc_farrow_generic.c)
set(eq-fir_sources eq_fir/eq_fir.c eq_fir/eq_fir_generic.c)
set(eq-iir_sources eq_iir/eq_iir.c)
set(dcblock_sources dcblock/dcblock.c dcblock/dcblock_generic.c dcblock/dcblock_hifi4.c)
set(crossover_sources crossover/crossover.c crossover/crossover_generic.c)
set(tdfb_sources tdfb/tdfb.c tdfb/tdfb_generic.c tdfb/tdfb_direction.c)
set(drc_sources drc/drc.c drc/drc_generic.c drc/drc_math_generic.c)
set(multiband_drc_sources multiband_drc/multiband_drc_generic.c crossover/crossover.c drc/drc.c drc/drc_generic.c drc/drc_math_generic.c multiband_drc/multiband_drc.c )
set(mfcc_sources mfcc/mfcc.c mfcc/mfcc_setup.c mfcc/mfcc_common.c mfcc/mfcc_generic.c mfcc/mfcc_hifi4.c mfcc/mfcc_hifi3.c)
set(mux_sources mux/mux.c mux/mux_generic.c)

foreach(audio_module ${sof_audio_modules})
	# first compile with no optimizations
	sof_audio_add_module(sof_${audio_module} "" ${${audio_module}_sources})

	# compile for each optimization
	foreach(opt ${available_optimizations})
		sof_audio_add_module(sof_${audio_module}_${opt} "${${opt}_flags}" ${${audio_module}_sources})
	endforeach()
endforeach()
