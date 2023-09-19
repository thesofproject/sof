# SPDX-License-Identifier: BSD-3-Clause

# PCM ALSA module
add_library(asound_module_pcm_sof MODULE
	pcm.c
	plugin.c
	../common.c
	tplg.c
)

sof_append_relative_path_definitions(asound_module_pcm_sof)

target_include_directories(asound_module_pcm_sof PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/..
	${sof_source_directory}/src/audio)

target_compile_options(asound_module_pcm_sof PRIVATE -DPIC -g -O3 -Wl,-EL -Wmissing-prototypes
  -Wimplicit-fallthrough -DCONFIG_LIBRARY -imacros${config_h})

install(TARGETS asound_module_pcm_sof DESTINATION /usr/lib/x86_64-linux-gnu/alsa-lib)

target_link_options(asound_module_pcm_sof PRIVATE -Wl,--export-dynamic,--no-undefined)
target_link_libraries(asound_module_pcm_sof PRIVATE sof_library)
target_link_libraries(asound_module_pcm_sof PRIVATE sof_parser_lib)
target_link_libraries(asound_module_pcm_sof PRIVATE pthread)
target_link_libraries(asound_module_pcm_sof PRIVATE  -lasound -ldl -lm -lasound -lrt)

target_include_directories(asound_module_pcm_sof PRIVATE ${sof_install_directory}/include)
target_include_directories(asound_module_pcm_sof PRIVATE ${parser_install_dir}/include)

set_target_properties(asound_module_pcm_sof
	PROPERTIES
	INSTALL_RPATH "${sof_install_directory}/alsa-lib"
	INSTALL_RPATH_USE_LINK_PATH TRUE
)

# CTL ALSA module
add_library(asound_module_ctl_sof MODULE
	ctl.c
	plugin.c
	../common.c
)
sof_append_relative_path_definitions(asound_module_ctl_sof)
target_include_directories(asound_module_ctl_sof PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/..
	${sof_source_directory}/src/audio)

target_compile_options(asound_module_ctl_sof PRIVATE -DPIC -g -O3 -Wl,-EL -Wmissing-prototypes
  -Wimplicit-fallthrough -Wall -Werror -DCONFIG_LIBRARY -imacros${config_h})

install(TARGETS asound_module_ctl_sof DESTINATION /usr/lib/x86_64-linux-gnu/alsa-lib)

target_link_options(asound_module_ctl_sof PRIVATE -Wl,--export-dynamic,--no-undefined)
target_link_libraries(asound_module_ctl_sof PRIVATE sof_library)
target_link_libraries(asound_module_ctl_sof PRIVATE sof_parser_lib)
target_link_libraries(asound_module_ctl_sof PRIVATE pthread)
target_link_libraries(asound_module_ctl_sof PRIVATE  -lasound -ldl -lm -lasound -lrt)

target_include_directories(asound_module_ctl_sof PRIVATE ${sof_install_directory}/include)
target_include_directories(asound_module_ctl_sof PRIVATE ${parser_install_dir}/include)

set_target_properties(asound_module_ctl_sof
	PROPERTIES
	INSTALL_RPATH "${sof_install_directory}/alsa-lib"
	INSTALL_RPATH_USE_LINK_PATH TRUE
)
