# SPDX-License-Identifier: BSD-3-Clause

add_executable(sof-pipe
	main.c
	cpu.c
	pipeline.c
	ctl.c
	ipc4.c
	../common.c
)

sof_append_relative_path_definitions(sof-pipe)

target_include_directories(sof-pipe PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/..
	${sof_source_directory}/src/audio)

target_compile_options(sof-pipe PRIVATE -DPIC -g -O3 -Wl,-EL -Wall -Werror -DCONFIG_LIBRARY -imacros${config_h})

target_include_directories(sof-pipe PRIVATE ${sof_install_directory}/include)
target_include_directories(sof-pipe PRIVATE ${parser_install_dir}/include)

target_link_libraries(sof-pipe PRIVATE -Wl,-whole-archive sof_library -Wl,-no-whole-archive)
target_link_libraries(sof-pipe PRIVATE sof_parser_lib)
target_link_libraries(sof-pipe PRIVATE pthread)
target_link_libraries(sof-pipe PRIVATE  -rdynamic -lasound -ldl -lm -lasound -lrt)
