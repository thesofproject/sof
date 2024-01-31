# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_IPC_MAJOR_3)
	set(mixer_src mixer/mixer.c mixer/mixer_generic.c mixer/mixer_hifi3.c)
elseif(CONFIG_IPC_MAJOR_4)
	set(mixer_src mixin_mixout/mixin_mixout.c mixin_mixout/mixin_mixout_generic.c mixin_mixout/mixin_mixout_hifi3.c)
endif()

if(CONFIG_COMP_MODULE_SHARED_LIBRARY_BUILD)
	include(native.cmake)
	return()
endif()

# Older indentation temporarily preserved to help git and git users.

	add_local_sources(sof
		host-legacy.c
		component.c
		buffer.c
		source_api_helper.c
		sink_api_helper.c
		sink_source_utils.c
		audio_stream.c
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
	if(CONFIG_COMP_DAI)
		add_local_sources(sof
			dai-legacy.c
		)
	endif()
	if(CONFIG_COMP_KPB AND NOT CONFIG_LIBRARY_STATIC)
		add_local_sources(sof
			kpb.c
		)
	endif()
	if(CONFIG_COMP_SEL)
		add_subdirectory(selector)
	endif()
	if(CONFIG_COMP_SMART_AMP)
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
	if(CONFIG_COMP_BASEFW_IPC4 AND NOT CONFIG_LIBRARY)
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
	if(CONFIG_COMP_MFCC)
		add_subdirectory(mfcc)
	endif()
	if(CONFIG_COMP_VOLUME)
		add_subdirectory(volume)
	endif()
	subdirs(pipeline)
	add_subdirectory(google)
	if(CONFIG_COMP_CHAIN_DMA)
		add_local_sources(sof chain_dma.c)
	endif()
	if(CONFIG_DTS_CODEC)
		add_subdirectory(codec)
	endif()
