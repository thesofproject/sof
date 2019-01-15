cmocka_test(buffer_copy
	buffer_copy.c
	mock.c
	${PROJECT_SOURCE_DIR}/src/audio/buffer.c
)

cmocka_test(buffer_new
	buffer_new.c
	mock.c
	${PROJECT_SOURCE_DIR}/src/audio/buffer.c
)

cmocka_test(buffer_wrap
	buffer_wrap.c
	mock.c
	${PROJECT_SOURCE_DIR}/src/audio/buffer.c
)

cmocka_test(buffer_write
	buffer_write.c
	mock.c
	${PROJECT_SOURCE_DIR}/src/audio/buffer.c
)
