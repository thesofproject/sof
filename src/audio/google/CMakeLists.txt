# SPDX-License-Identifier: BSD-3-Clause

if((NOT CONFIG_LIBRARY) OR CONFIG_LIBRARY_STATIC)
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

