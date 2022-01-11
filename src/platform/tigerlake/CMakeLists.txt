# SPDX-License-Identifier: BSD-3-Clause

add_subdirectory(lib)

add_executable(boot_module boot_module.c)
add_executable(base_module base_module.c)

target_link_libraries(boot_module sof_options)
target_link_libraries(base_module sof_options)

sof_append_relative_path_definitions(boot_module)
sof_append_relative_path_definitions(base_module)
