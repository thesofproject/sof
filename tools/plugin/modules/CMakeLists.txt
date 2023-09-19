# SPDX-License-Identifier: BSD-3-Clause

# File SHM pipe module
add_library(sof_mod_shm MODULE
	shm.c
)
sof_append_relative_path_definitions(sof_mod_shm)
target_include_directories(sof_mod_shm PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/..
	${CMAKE_CURRENT_SOURCE_DIR}/../pipe
	${sof_source_directory}/src/audio)

target_compile_options(sof_mod_shm PRIVATE -DPIC -g -O3 -Wl,-EL -Wall -Werror -DCONFIG_LIBRARY -imacros${config_h})

install(TARGETS sof_mod_shm
		DESTINATION /usr/lib/x86_64-linux-gnu/alsa-lib)

target_link_options(sof_mod_shm PRIVATE -Wl,--export-dynamic)

target_include_directories(sof_mod_shm PRIVATE ${sof_install_directory}/include)
target_include_directories(sof_mod_shm PRIVATE ${parser_install_dir}/include)

set_target_properties(sof_mod_shm
	PROPERTIES
	INSTALL_RPATH "${sof_install_directory}/alsa-lib"
	INSTALL_RPATH_USE_LINK_PATH TRUE
)


# ALSA SOF pipe module
add_library(sof_mod_alsa MODULE
	alsa.c
)
sof_append_relative_path_definitions(sof_mod_alsa)
target_include_directories(sof_mod_alsa PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/..
	${CMAKE_CURRENT_SOURCE_DIR}/../pipe
	${sof_source_directory}/src/audio)

target_compile_options(sof_mod_alsa PRIVATE -DPIC -g -O3 -Wl,-EL -Wmissing-prototypes
  -Wimplicit-fallthrough -Wall -Werror -DCONFIG_LIBRARY -imacros${config_h})

install(TARGETS sof_mod_alsa
		DESTINATION /usr/lib/x86_64-linux-gnu/alsa-lib)

target_link_options(sof_mod_alsa PRIVATE -Wl,--export-dynamic)

target_include_directories(sof_mod_alsa PRIVATE ${sof_install_directory}/include)
target_include_directories(sof_mod_alsa PRIVATE ${parser_install_dir}/include)

set_target_properties(sof_mod_alsa
	PROPERTIES
	INSTALL_RPATH "${sof_install_directory}/alsa-lib"
	INSTALL_RPATH_USE_LINK_PATH TRUE
)
