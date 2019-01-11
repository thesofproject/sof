add_local_sources(sof power_down.S)

add_executable(boot_module boot_module.c)
add_executable(base_module base_module.c)

target_link_libraries(boot_module sof_options)
target_link_libraries(base_module sof_options)
