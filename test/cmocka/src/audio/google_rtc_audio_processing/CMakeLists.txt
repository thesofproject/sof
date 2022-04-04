# SPDX-License-Identifier: BSD-3-Clause

cmocka_test(google_rtc_audio_processing_process
  google_rtc_audio_processing_process.c
  )

# make small version of libaudio so we don't have to care
# about unused missing references
add_compile_options(-DUNIT_TEST)

set(GOOGLE_RTC_AUDIO_PROCESSING_UT_SRC
  ${PROJECT_SOURCE_DIR}/src/audio/google_rtc_audio_processing.c
  ${PROJECT_SOURCE_DIR}/src/audio/google_rtc_audio_processing_mock.c
  ${PROJECT_SOURCE_DIR}/src/audio/component.c
  ${PROJECT_SOURCE_DIR}/src/audio/data_blob.c
  ${PROJECT_SOURCE_DIR}/src/ipc/ipc-helper.c
  ${PROJECT_SOURCE_DIR}/src/audio/buffer.c
  ${PROJECT_SOURCE_DIR}/src/ipc/ipc-common.c
  ${PROJECT_SOURCE_DIR}/src/ipc/ipc3/helper.c
  ${PROJECT_SOURCE_DIR}/test/cmocka/src/notifier_mocks.c
  ${PROJECT_SOURCE_DIR}/src/audio/pipeline/pipeline-graph.c
  ${PROJECT_SOURCE_DIR}/src/audio/pipeline/pipeline-params.c
  ${PROJECT_SOURCE_DIR}/src/audio/pipeline/pipeline-schedule.c
  ${PROJECT_SOURCE_DIR}/src/audio/pipeline/pipeline-stream.c
  ${PROJECT_SOURCE_DIR}/src/audio/pipeline/pipeline-xrun.c
  ${PROJECT_SOURCE_DIR}/src/platform/library/platform.c
  )

if(CONFIG_GOOGLE_RTC_AUDIO_PROCESSING_MOCK)
  message(STATUS "Building Google RTC Audio processing unit test with MOCK")
  list(APPEND GOOGLE_RTC_AUDIO_PROCESSING_UT_SRC
	${PROJECT_SOURCE_DIR}/src/audio/google_rtc_audio_processing_mock.c)
endif()

add_library(audio_for_google_rtc_audio_processing STATIC
  ${GOOGLE_RTC_AUDIO_PROCESSING_UT_SRC}
  )
sof_append_relative_path_definitions(audio_for_google_rtc_audio_processing)

target_include_directories(audio_for_google_rtc_audio_processing PRIVATE
  ${PROJECT_SOURCE_DIR}/third_party/include
  ${PROJECT_SOURCE_DIR}/src/audio)

if(NOT CONFIG_GOOGLE_RTC_AUDIO_PROCESSING_MOCK)
  target_link_directories(audio_for_google_rtc_audio_processing PUBLIC ${PROJECT_SOURCE_DIR}/third_party/lib)
  target_link_libraries(audio_for_google_rtc_audio_processing PRIVATE c++ c++abi m c)
  target_link_libraries(google_rtc_audio_processing_process PRIVATE audio_for_google_rtc_audio_processing)
  target_link_libraries(google_rtc_audio_processing_process PRIVATE google_rtc_audio_processing google_rtc_audio_processing_tuning)
endif()
target_link_libraries(audio_for_google_rtc_audio_processing PRIVATE sof_options)
target_link_libraries(google_rtc_audio_processing_process PRIVATE audio_for_google_rtc_audio_processing)
