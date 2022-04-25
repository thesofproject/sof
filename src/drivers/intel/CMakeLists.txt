# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_BAYTRAIL OR CONFIG_CHERRYTRAIL)
	add_local_sources(sof pmc-ipc.c)
	add_subdirectory(baytrail)
elseif(CONFIG_HASWELL OR CONFIG_BROADWELL)
	add_subdirectory(haswell)
elseif(CONFIG_CAVS)
	add_subdirectory(cavs)
endif()

if(CONFIG_INTEL_HDA)
	add_subdirectory(hda)
endif()

if(CONFIG_INTEL_SSP)
	add_subdirectory(ssp)
endif()

if(CONFIG_INTEL_ALH)
	add_subdirectory(alh)
endif()

if(CONFIG_INTEL_DMIC)
	add_subdirectory(dmic)
endif()
