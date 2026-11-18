# SPDX-License-Identifier: BSD-3-Clause

sof_add_include_directory(include)

add_subdirectory(lib)

sof_add_files(
	platform.c
)

if(CONFIG_PLATFORM_NRF54L15)
	sof_add_files(nrf54l15_pipeline_def.c)
elseif(CONFIG_PLATFORM_NRF54LM20)
	sof_add_files(nrf54lm20_pipeline_def.c)
endif()
