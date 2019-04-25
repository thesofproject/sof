set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

add_executable(testbench
	testbench.c
	alloc.c
	common_test.c
	file.c
	ipc.c
	schedule.c
	edf_schedule.c
	ll_schedule.c
	panic.c
	topology.c
	trace.c
)

target_include_directories(testbench PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(testbench PRIVATE -ldl -lm)
target_link_libraries(testbench PRIVATE sof sof_options)

install(TARGETS testbench DESTINATION bin)
