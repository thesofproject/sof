if(NOT BUILD_HOST)
	add_local_sources(sof
		switch.c
		dai.c
		host.c
		pipeline.c
		pipeline_static.c
		component.c
		buffer.c
	)
	if(CONFIG_COMP_VOLUME)
		add_local_sources(sof
			volume.c
			volume_generic.c
			volume_hifi3.c
		)
	endif()
	if(CONFIG_COMP_SRC)
		add_local_sources(sof
			src.c
			src_generic.c
			src_hifi2ep.c
			src_hifi3.c
		)
	endif()
	if(CONFIG_COMP_FIR)
		add_local_sources(sof
			eq_fir.c
			fir.c
			fir_hifi2ep.c
			fir_hifi3.c
		)
	endif()
	if(CONFIG_COMP_IIR)
		add_local_sources(sof
			eq_iir.c
			iir.c
		)
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
		add_local_sources(sof
			mux.c
		)
	endif()
	return()
endif()

add_library(sof_audio_core SHARED "")
target_link_libraries(sof_audio_core PRIVATE sof_options)
target_link_libraries(sof_audio_core PRIVATE -Wl,--export-dynamic)
add_local_sources(sof_audio_core
	dai.c
	host.c
	pipeline.c
	component.c
	buffer.c
)

install(TARGETS sof_audio_core DESTINATION lib)

# Audio Modules with various optimizaitons

# add rules for module compilation and installation
function(sof_audio_add_module lib_name compile_flags)
	add_library(${lib_name} MODULE "")
	target_link_libraries(${lib_name} PRIVATE sof_options)
	target_link_libraries(${lib_name} PRIVATE -Wl,--export-dynamic)
	target_compile_options(${lib_name} PRIVATE ${compile_flags})
	add_local_sources(${lib_name} ${ARGN})
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

set(sof_audio_modules volume src)

# sources for each module
set(volume_sources volume.c volume_generic.c)
set(src_sources src.c src_generic.c)

foreach(audio_module ${sof_audio_modules})
	# first compile with no optimizations
	sof_audio_add_module(sof_${audio_module} "" ${${audio_module}_sources})

	# compile for each optimization
	foreach(opt ${available_optimizations})
		sof_audio_add_module(sof_${audio_module}_${opt} "${${opt}_flags}" ${${audio_module}_sources})
	endforeach()
endforeach()
