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

### Common actions for Zephyr and the host library ###
# directories included unconditionally (alphabetical order)
add_subdirectory(buffers)
add_subdirectory(pcm_converter)
add_subdirectory(pipeline)

if(CONFIG_COMP_BASEFW_IPC4 AND NOT CONFIG_LIBRARY)
	add_local_sources(sof base_fw.c)
endif()
add_local_sources_ifdef(CONFIG_IPC4_BASE_FW_INTEL sof base_fw_intel.c)

# directories and files included conditionally (alphabetical order)
if(CONFIG_COMP_ARIA)
	add_subdirectory(aria)
endif()
if(CONFIG_COMP_ASRC)
	add_subdirectory(asrc)
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
if(CONFIG_COMP_IGO_NR)
	add_subdirectory(igo_nr)
endif()
if(CONFIG_COMP_IIR)
	add_subdirectory(eq_iir)
endif()
if(CONFIG_COMP_LEVEL_MULTIPLIER)
	add_subdirectory(level_multiplier)
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
if(CONFIG_COMP_PHASE_VOCODER)
	add_subdirectory(phase_vocoder)
endif()
if(CONFIG_COMP_RTNR)
	add_subdirectory(rtnr)
endif()
if(CONFIG_COMP_SEL)
	add_subdirectory(selector)
endif()
if(CONFIG_COMP_SMART_AMP)
	add_subdirectory(smart_amp)
endif()
if(CONFIG_COMP_SOUND_DOSE)
	add_subdirectory(sound_dose)
endif()
if(CONFIG_COMP_SRC)
	add_subdirectory(src)
endif()
if(CONFIG_COMP_STFT_PROCESS)
	add_subdirectory(stft_process)
endif()
if(CONFIG_COMP_TDFB)
	add_subdirectory(tdfb)
endif()
if(CONFIG_COMP_TEMPLATE)
	add_subdirectory(template)
endif()
if(CONFIG_COMP_TENSORFLOW)
	add_subdirectory(tensorflow)
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
# end of directories and files included conditionally (alphabetical order)

add_subdirectory(google)
add_subdirectory(nxp)

if(CONFIG_COMP_CHAIN_DMA)
	add_local_sources(sof chain_dma.c)
endif()
if(CONFIG_COMP_KPB AND NOT CONFIG_LIBRARY)
	add_local_sources(sof
		kpb.c
	)
endif()
if(CONFIG_INTEL_ADSP_MIC_PRIVACY)
	add_subdirectory(mic_privacy_manager)
endif()
if(CONFIG_COMP_TONE)
	add_subdirectory(tone)
endif()
if(CONFIG_UAOL_INTEL_ADSP)
	add_local_sources(sof intel_uaol.c)
endif()
if(CONFIG_ZEPHYR_NATIVE_DRIVERS)
	list(APPEND base_files host-zephyr.c)
	sof_list_append_ifdef(CONFIG_COMP_DAI base_files dai-zephyr.c)
else()
	list(APPEND base_files host-legacy.c)
	sof_list_append_ifdef(CONFIG_COMP_DAI base_files dai-legacy.c)
endif()

### Common files

sof_list_append_ifdef(CONFIG_COMP_BLOB base_files data_blob.c)

### (end of common files)

is_zephyr(zephyr)
if(zephyr) ###  Zephyr ###

	zephyr_library_sources(
		${base_files}
	)

else() ### library, e.g. testbench ###

	add_local_sources(sof
		host-legacy.c
		${base_files}
	)

	if(CONFIG_COMP_DAI)
		add_local_sources(sof
			dai-legacy.c
		)
	endif()
endif()
