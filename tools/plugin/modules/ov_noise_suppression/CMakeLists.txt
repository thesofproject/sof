# SPDX-License-Identifier: BSD-3-Clause

# Noise suppression module
find_package(OpenVINO COMPONENTS Runtime)

if(OpenVINO_FOUND)
set(CMAKE_CXX_STANDARD 17)

add_library(sof_ns_interface STATIC noise_suppression_interface.cpp)
target_link_libraries(sof_ns_interface PRIVATE -Wl,--export-dynamic)
target_link_libraries(sof_ns_interface PRIVATE openvino::runtime)
target_compile_options(sof_ns_interface PRIVATE -fPIC)
target_include_directories(sof_ns_interface PRIVATE ${sof_source_directory}/src/include)
target_include_directories(sof_ns_interface PRIVATE ${sof_source_directory}/posix/include)
target_include_directories(sof_ns_interface PRIVATE ${sof_source_directory}/src/arch/host/include)
target_include_directories(sof_ns_interface PRIVATE ${sof_source_directory}/src/platform/posix/include)
set_target_properties(sof_ns_interface PROPERTIES LINKER_LANGUAGE CXX)
sof_append_relative_path_definitions(sof_ns_interface)
install(TARGETS sof_ns_interface DESTINATION /usr/lib/x86_64-linux-gnu/alsa-lib)

add_library(sof_ns MODULE noise_suppression.c)
target_link_libraries(sof_ns PRIVATE sof_ns_interface)
target_link_libraries(sof_ns PRIVATE -Wl,--export-dynamic)
sof_append_relative_path_definitions(sof_ns)
target_compile_options(sof_ns PRIVATE -DPIC -g -O3 -Wl,-EL -Wall -Werror -DCONFIG_LIBRARY -imacros${config_h})
install(TARGETS sof_ns DESTINATION /usr/lib/x86_64-linux-gnu/alsa-lib)

target_include_directories(sof_ns PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/..
	${CMAKE_CURRENT_SOURCE_DIR}/../pipe
	${sof_source_directory}/src/audio)


target_link_options(sof_ns PRIVATE -Wl,--export-dynamic)
target_include_directories(sof_ns PRIVATE ${sof_install_directory}/include)
target_include_directories(sof_ns PRIVATE ${parser_install_dir}/include)

set_target_properties(sof_ns
	PROPERTIES
	INSTALL_RPATH "${sof_install_directory}/alsa-lib"
	INSTALL_RPATH_USE_LINK_PATH TRUE
)
endif()
