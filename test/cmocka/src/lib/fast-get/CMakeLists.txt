# SPDX-License-Identifier: BSD-3-Clause

cmocka_test(fast-get-tests
	fast-get-tests.c
	${PROJECT_SOURCE_DIR}/zephyr/lib/fast-get.c
	${PROJECT_SOURCE_DIR}/src/lib/alloc.c
	${PROJECT_SOURCE_DIR}/src/platform/library/lib/memory.c
	${PROJECT_SOURCE_DIR}/src/spinlock.c
)

target_link_libraries(fast-get-tests PRIVATE "-Wl,--wrap=rzalloc,--wrap=rmalloc,--wrap=rfree")
