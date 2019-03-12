set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

add_executable(testbench "")
add_local_sources(testbench testbench.c)

add_library(tb_common STATIC "")
target_link_libraries(testbench PRIVATE -ldl -lm)
target_link_libraries(testbench PRIVATE sof_ipc sof_audio_core tb_common)


target_link_libraries(tb_common sof_options)
add_local_sources(tb_common
	alloc.c
	common_test.c
	file.c
	ipc.c
	schedule.c
	edf_schedule.c
	ll_schedule.c
	topology.c
	trace.c
)

install(TARGETS testbench DESTINATION bin)
