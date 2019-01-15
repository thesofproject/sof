cmocka_test(gcd
	gcd.c
	${PROJECT_SOURCE_DIR}/src/math/numbers.c
)

cmocka_test(ceil_divide
	ceil_divide.c
	${PROJECT_SOURCE_DIR}/src/math/numbers.c
)
target_link_libraries(ceil_divide PRIVATE -lm)

cmocka_test(find_equal_int16
	find_equal_int16.c
	${PROJECT_SOURCE_DIR}/src/math/numbers.c
)

cmocka_test(find_min_int16
	find_min_int16.c
	${PROJECT_SOURCE_DIR}/src/math/numbers.c
)

cmocka_test(find_max_abs_int32
	find_max_abs_int32.c
	${PROJECT_SOURCE_DIR}/src/math/numbers.c
)

cmocka_test(norm_int32
	norm_int32.c
	${PROJECT_SOURCE_DIR}/src/math/numbers.c
)
