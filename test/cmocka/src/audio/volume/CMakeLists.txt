cmocka_test(volume_process
	volume_process.c
)

target_include_directories(volume_process PRIVATE ${PROJECT_SOURCE_DIR}/src/audio)

# make small version of libaudio so we don't have to care
# about unused missing references
add_library(audio_for_volume STATIC
	${PROJECT_SOURCE_DIR}/src/audio/volume.c
	${PROJECT_SOURCE_DIR}/src/audio/volume_generic.c
	${PROJECT_SOURCE_DIR}/src/audio/volume_hifi3.c
)

target_link_libraries(audio_for_volume PRIVATE sof_options)

target_link_libraries(volume_process PRIVATE audio_for_volume)
