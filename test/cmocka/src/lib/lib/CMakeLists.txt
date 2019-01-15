cmocka_test(rstrcmp
	rstrcmp.c
	${PROJECT_SOURCE_DIR}/src/lib/lib.c
)

cmocka_test(rstrlen
	rstrlen.c
	${PROJECT_SOURCE_DIR}/src/lib/lib.c
)

cmocka_test(strcheck
	strcheck.c
	${PROJECT_SOURCE_DIR}/src/lib/lib.c
)
