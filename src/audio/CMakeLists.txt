# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_IPC_MAJOR_3)
	set(mixer_src mixer/mixer.c mixer/mixer_generic.c mixer/mixer_hifi3.c)
elseif(CONFIG_IPC_MAJOR_4)
	set(mixer_src mixin_mixout.c)
endif()

if(NOT CONFIG_LIBRARY)
	add_local_sources(sof
		host-legacy.c
		component.c
		buffer.c
		channel_map.c
	)
	if(CONFIG_COMP_BLOB)
		add_local_sources(sof data_blob.c)
	endif()
	if(CONFIG_COMP_SRC)
		add_subdirectory(src)
	endif()
	if(CONFIG_COMP_FIR)
		add_subdirectory(eq_fir)
	endif()
	if(CONFIG_COMP_IIR)
		add_subdirectory(eq_iir)
	endif()
	if(CONFIG_COMP_DCBLOCK)
		add_subdirectory(dcblock)
	endif()
	if(CONFIG_COMP_CROSSOVER)
		add_subdirectory(crossover)
	endif()
	if(CONFIG_COMP_TDFB)
		add_subdirectory(tdfb)
	endif()
	if(CONFIG_COMP_DRC)
		add_subdirectory(drc)
	endif()
	if(CONFIG_COMP_MULTIBAND_DRC)
		add_subdirectory(multiband_drc)
	endif()
	if(CONFIG_COMP_TONE)
		add_local_sources(sof
			tone.c
		)
	endif()
	if(CONFIG_COMP_MIXER)
		add_local_sources(sof
			${mixer_src}
		)
	endif()
	if(CONFIG_COMP_MUX)
		add_subdirectory(mux)
	endif()
	if(CONFIG_COMP_SWITCH)
		add_local_sources(sof
			switch.c
		)
	endif()
	if(CONFIG_COMP_DAI)
		add_local_sources(sof
			dai-legacy.c
		)
	endif()
	if(CONFIG_COMP_KPB)
		add_local_sources(sof
			kpb.c
		)
	endif()
	if(CONFIG_COMP_GOOGLE_HOTWORD_DETECT)
		add_local_sources(sof
			google_hotword_detect.c
		)
		target_link_libraries(sof PRIVATE libhifi3_google_hotword_dsp_api.a)
		target_link_libraries(sof PRIVATE c)
		target_link_libraries(sof PRIVATE m)
	endif()

	if(CONFIG_COMP_GOOGLE_RTC_AUDIO_PROCESSING)
	  target_include_directories(sof PRIVATE ${CMAKE_SOURCE_DIR}/third_party/include)
	  add_local_sources(sof
		google_rtc_audio_processing.c
		)
	  if(CONFIG_GOOGLE_RTC_AUDIO_PROCESSING_MOCK)
		add_local_sources(sof
		  google_rtc_audio_processing_mock.c
		  )
	  else()
		message(INFO "Link with google_rtc_audio_processing")
		target_link_directories(sof PRIVATE ${CMAKE_SOURCE_DIR}/third_party/lib)
		target_link_libraries(sof PRIVATE google_rtc_audio_processing)
		target_link_libraries(sof PRIVATE c++)
		target_link_libraries(sof PRIVATE c++abi)
		target_link_libraries(sof PRIVATE m)
		target_link_libraries(sof PRIVATE c)
	  endif()
	endif()

	if(CONFIG_COMP_GOOGLE_AUDIO_POST_PROCESSING)
	  target_include_directories(sof PRIVATE ${CMAKE_SOURCE_DIR}/third_party/include)
	  add_local_sources(sof
		google_audio_post_processing.c
		)
	  if(CONFIG_GOOGLE_AUDIO_POST_PROCESSING_MOCK)
		add_local_sources(sof
		  google_audio_post_processing_mock.c
		  )
	  else()
		message(INFO "Link with google_audio_post_processing")
		target_link_directories(sof PRIVATE ${CMAKE_SOURCE_DIR}/third_party/lib)
		target_link_libraries(sof PRIVATE google_audio_post_processing)
		target_link_libraries(sof PRIVATE c++)
		target_link_libraries(sof PRIVATE c++abi)
		target_link_libraries(sof PRIVATE m)
		target_link_libraries(sof PRIVATE c)
	  endif()
	endif()

	if(CONFIG_COMP_SEL)
		add_subdirectory(selector)
	endif()
	if(CONFIG_MAXIM_DSM)
		sof_add_static_library(dsm ./smart_amp/dsm_lib/libdsm.a)
		add_subdirectory(smart_amp)
	endif()
	add_subdirectory(pcm_converter)
	if(CONFIG_COMP_ASRC)
		add_subdirectory(asrc)
	endif()
	if(CONFIG_COMP_MODULE_ADAPTER)
		add_subdirectory(module_adapter)
	endif()
	if(CONFIG_COMP_IGO_NR)
		add_subdirectory(igo_nr)
	endif()
	if(CONFIG_COMP_COPIER)
		add_subdirectory(copier)
	endif()
	if(CONFIG_COMP_RTNR)
		add_subdirectory(rtnr)
	endif()
	if(CONFIG_COMP_BASEFW_IPC4)
                add_local_sources(sof
                        base_fw.c
                )
        endif()
	if(CONFIG_COMP_ARIA)
		add_subdirectory(aria)
	endif()
	if(CONFIG_COMP_UP_DOWN_MIXER)
		add_subdirectory(up_down_mixer)
	endif()
	subdirs(pipeline)

	return()
endif()

subdirs(pipeline)

add_local_sources(sof
	component.c
	data_blob.c
	buffer.c
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

set(sof_audio_modules mixer volume src asrc eq-fir eq-iir dcblock crossover tdfb drc multiband_drc)

# sources for each module
set(volume_sources module_adapter/module_adapter.c module_adapter/module/generic.c module_adapter/module/volume/volume.c module_adapter/module/volume/volume_generic.c)
set(mixer_sources ${mixer_src})
set(src_sources src/src.c src/src_generic.c)
set(asrc_sources asrc/asrc.c asrc/asrc_farrow.c asrc/asrc_farrow_generic.c)
set(eq-fir_sources eq_fir/eq_fir.c eq_fir/eq_fir_generic.c)
set(eq-iir_sources eq_iir/eq_iir.c)
set(dcblock_sources dcblock/dcblock.c dcblock/dcblock_generic.c)
set(crossover_sources crossover/crossover.c crossover/crossover_generic.c)
set(tdfb_sources tdfb/tdfb.c tdfb/tdfb_generic.c tdfb/tdfb_direction.c)
set(drc_sources drc/drc.c drc/drc_generic.c drc/drc_math_generic.c)
set(multiband_drc_sources multiband_drc/multiband_drc_generic.c crossover/crossover.c crossover/crossover_generic.c drc/drc.c drc/drc_generic.c drc/drc_math_generic.c multiband_drc/multiband_drc.c )

foreach(audio_module ${sof_audio_modules})
	# first compile with no optimizations
	sof_audio_add_module(sof_${audio_module} "" ${${audio_module}_sources})

	# compile for each optimization
	foreach(opt ${available_optimizations})
		sof_audio_add_module(sof_${audio_module}_${opt} "${${opt}_flags}" ${${audio_module}_sources})
	endforeach()
endforeach()
