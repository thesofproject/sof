if(CONFIG_GOOGLE_RTC_AUDIO_PROCESSING_MOCK)
sof_llext_build("google_rtc_audio_processing"
	SOURCES ../google_rtc_audio_processing.c
		../google_rtc_audio_processing_mock.c
)
target_include_directories(google_rtc_audio_processing_llext_lib PRIVATE
	"${sof_top_dir}/third_party/include"
)
else()
message(FATAL_ERROR "Add library linking support in src/audio/google/llext/CMakeLists.txt")
endif()
