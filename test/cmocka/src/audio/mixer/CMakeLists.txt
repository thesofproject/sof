cmocka_test(mixer
	mixer_test.c
	mock.c
	comp_mock.c
	${PROJECT_SOURCE_DIR}/src/audio/buffer.c
	${PROJECT_SOURCE_DIR}/src/audio/mixer.c
)
target_link_libraries(mixer PRIVATE -lm)
