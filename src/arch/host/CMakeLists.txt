# includes
target_include_directories(sof_options INTERFACE ${PROJECT_SOURCE_DIR}/src/arch/host/include)
target_include_directories(sof_options INTERFACE ${PROJECT_SOURCE_DIR}/src/platform/library/include)

# linker flags
target_link_libraries(sof_options INTERFACE -lpthread)

# C & ASM flags
target_compile_options(sof_options INTERFACE -g -O3 -Wall -Werror -Wl,-EL -Wmissing-prototypes -Wimplicit-fallthrough=3)
