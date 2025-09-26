# SPDX-License-Identifier: BSD-3-Clause

is_zephyr(zephyr)
if(zephyr) ###  Zephyr ###

	set(THIRD_PARTY ${SOF_ROOT_SOURCE_DIRECTORY}/third_party)
	if(CONFIG_IPC_MAJOR_3)
		set(ipc_suffix ipc3)
	elseif(CONFIG_IPC_MAJOR_4)
		set(ipc_suffix ipc4)
	endif()

	zephyr_library_sources_ifdef(CONFIG_COMP_GOOGLE_HOTWORD_DETECT
		google_hotword_detect.c
	)

	if(CONFIG_COMP_GOOGLE_RTC_AUDIO_PROCESSING STREQUAL "m" AND DEFINED CONFIG_LLEXT)
		add_subdirectory(llext_rtc
				 ${PROJECT_BINARY_DIR}/google_rtc_audio_processing_llext)
		add_dependencies(app google_rtc_audio_processing)
	elseif(CONFIG_COMP_GOOGLE_RTC_AUDIO_PROCESSING)
		zephyr_library_sources(google_rtc_audio_processing.c)

		zephyr_library_sources_ifdef(CONFIG_GOOGLE_RTC_AUDIO_PROCESSING_MOCK
			google_rtc_audio_processing_mock.c
		)

		if(NOT CONFIG_GOOGLE_RTC_AUDIO_PROCESSING_MOCK)
			zephyr_include_directories(${THIRD_PARTY}/include)
			target_link_directories(SOF INTERFACE ${THIRD_PARTY}/lib)
			target_link_libraries(SOF INTERFACE google_rtc_audio_processing)
			target_link_libraries(SOF INTERFACE c++)
			target_link_libraries(SOF INTERFACE c++abi)
			target_link_libraries(SOF INTERFACE m)
			target_link_libraries(SOF INTERFACE c)
			target_link_libraries(SOF INTERFACE gcc)
		endif()
	endif()

	if(CONFIG_COMP_GOOGLE_CTC_AUDIO_PROCESSING STREQUAL "m" AND DEFINED CONFIG_LLEXT)
		add_subdirectory(llext_ctc
				 ${PROJECT_BINARY_DIR}/google_ctc_audio_processing_llext)
		add_dependencies(app google_ctc_audio_processing)
	elseif(CONFIG_COMP_GOOGLE_CTC_AUDIO_PROCESSING)
		zephyr_library_sources(
			google_ctc_audio_processing.c
			google_ctc_audio_processing_${ipc_suffix}.c
		)
		zephyr_library_sources_ifdef(CONFIG_GOOGLE_CTC_AUDIO_PROCESSING_MOCK
			google_ctc_audio_processing_mock.c
		)

		if(NOT CONFIG_GOOGLE_CTC_AUDIO_PROCESSING_MOCK)
			zephyr_include_directories(${THIRD_PARTY}/include)
			target_link_directories(SOF INTERFACE ${THIRD_PARTY}/lib)
			target_link_libraries(SOF INTERFACE google_ctc_audio_processing)
			target_link_libraries(SOF INTERFACE c++)
			target_link_libraries(SOF INTERFACE c++abi)
			target_link_libraries(SOF INTERFACE m)
			target_link_libraries(SOF INTERFACE c)
			target_link_libraries(SOF INTERFACE gcc)
		endif()
	endif()

	return()
endif()

### Library build rules ###
if(CONFIG_LIBRARY_STATIC)
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

	if(CONFIG_COMP_GOOGLE_CTC_AUDIO_PROCESSING)
	  target_include_directories(sof PRIVATE ${CMAKE_SOURCE_DIR}/third_party/include)
	  add_local_sources(sof
		google_ctc_audio_processing.c
		)
	  if(CONFIG_IPC_MAJOR_3)
	  	add_local_sources(sof google_ctc_audio_processing_ipc3.c)
	  elseif(CONFIG_IPC_MAJOR_4)
	  	add_local_sources(sof google_ctc_audio_processing_ipc4.c)
	  endif()
	  if(CONFIG_GOOGLE_CTC_AUDIO_PROCESSING_MOCK)
		add_local_sources(sof
		  google_ctc_audio_processing_mock.c
		  )
	  else()
		message(INFO "Link with google_ctc_audio_processing")
		target_link_directories(sof PRIVATE ${CMAKE_SOURCE_DIR}/third_party/lib)
		target_link_libraries(sof PRIVATE google_ctc_audio_processing)
		target_link_libraries(sof PRIVATE c++)
		target_link_libraries(sof PRIVATE c++abi)
		target_link_libraries(sof PRIVATE m)
		target_link_libraries(sof PRIVATE c)
	  endif()
	endif()
	return()
endif()

