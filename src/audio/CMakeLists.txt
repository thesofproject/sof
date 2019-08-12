# SPDX-License-Identifier: BSD-3-Clause

if(NOT BUILD_LIBRARY)
	add_local_sources(sof
		host.c
		pipeline.c
		pipeline_static.c
		component.c
		buffer.c
		channel_map.c
	)
	if(CONFIG_COMP_VOLUME)
		add_subdirectory(volume)
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
	if(CONFIG_COMP_TONE)
		add_local_sources(sof
			tone.c
		)
	endif()
	if(CONFIG_COMP_MIXER)
		add_local_sources(sof
			mixer.c
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
			dai.c
		)
	endif()
	if(CONFIG_COMP_KPB)
		add_local_sources(sof
			kpb.c
		)
	endif()
	if(CONFIG_COMP_SEL)
		add_subdirectory(selector)
	endif()
	if(CONFIG_COMP_TEST_KEYPHRASE)
		add_local_sources(sof
			detect_test.c
		)
	endif()
	if(CONFIG_COMP_SMART_AMP)
		add_local_sources(sof
			smart_amp.c
		)
	endif()
	add_subdirectory(pcm_converter)
	if(CONFIG_COMP_ASRC)
		add_subdirectory(asrc)
	endif()
	return()
endif()

add_local_sources(sof
	pipeline.c
	component.c
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
macro(check_optimization opt_name flag extra_define)
	check_c_compiler_flag(${flag} compiles_flag_${opt_name})
	if(compiles_flag_${opt_name})
		list(APPEND available_optimizations ${opt_name})
		set(${opt_name}_flags ${flag} ${extra_define} -ffast-math -ftree-vectorizer-verbose=0)
	endif()
endmacro()

# modules will be compiled only for flags supported by compiler
check_optimization(sse42 -msse4.2 -DOPS_SSE42)
check_optimization(avx -mavx -DOPS_AVX)
check_optimization(avx2 -mavx2 -DOPS_AVX2)
check_optimization(fma -mfma -DOPS_FMA)
check_optimization(hifi2ep -mhifi2ep -DOPS_HIFI2EP)
check_optimization(hifi3 -mhifi3 -DOPS_HIFI3)

set(sof_audio_modules volume src asrc eq-fir eq-iir)

# sources for each module
set(volume_sources volume/volume.c volume/volume_generic.c)
set(src_sources src/src.c src/src_generic.c)
set(asrc_sources asrc/asrc.c asrc/asrc_farrow.c asrc/asrc_farrow_generic.c)
set(eq-fir_sources eq_fir/eq_fir.c eq_fir/fir.c)
set(eq-iir_sources eq_iir/eq_iir.c eq_iir/iir.c eq_iir/iir_generic.c)

foreach(audio_module ${sof_audio_modules})
	# first compile with no optimizations
	sof_audio_add_module(sof_${audio_module} "" ${${audio_module}_sources})

	# compile for each optimization
	foreach(opt ${available_optimizations})
		sof_audio_add_module(sof_${audio_module}_${opt} "${${opt}_flags}" ${${audio_module}_sources})
	endforeach()
endforeach()
