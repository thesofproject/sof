# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_CAVS)
	add_subdirectory(cavs)
endif()

if(CONFIG_INTEL_HDA)
	add_subdirectory(hda)
endif()

if(CONFIG_INTEL_SSP)
	add_subdirectory(ssp)
endif()

if(CONFIG_INTEL_ALH)
	add_local_sources(sof alh.c)
endif()

if(CONFIG_INTEL_DMIC)
	add_subdirectory(dmic)
endif()
