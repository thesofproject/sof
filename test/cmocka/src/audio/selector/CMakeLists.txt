cmocka_test(selector_test
	selector_test.c
)

target_include_directories(selector_test PRIVATE ${PROJECT_SOURCE_DIR}/src/audio)

# make small version of libaudio so we don't have to care
# about unused missing references
add_library(audio_for_selector STATIC
	${PROJECT_SOURCE_DIR}/src/audio/selector.c
	${PROJECT_SOURCE_DIR}/src/audio/selector_generic.c
)

target_link_libraries(audio_for_selector PRIVATE sof_options)

target_link_libraries(selector_test PRIVATE audio_for_selector)
