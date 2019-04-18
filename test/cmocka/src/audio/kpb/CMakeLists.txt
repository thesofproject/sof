cmocka_test(kpb
	${PROJECT_SOURCE_DIR}/src/audio/kpb.c
	kpb_buffer.c
	kpb_mock.c
	${PROJECT_SOURCE_DIR}/src/audio/buffer.c
	#${PROJECT_SOURCE_DIR}/src/audio/component.c
)
target_link_libraries(kpb PRIVATE -lm)
