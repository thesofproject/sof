# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_BAYTRAIL OR CONFIG_CHERRYTRAIL)
	add_local_sources(sof pmc-ipc.c)
	add_subdirectory(baytrail)
elseif(CONFIG_HASWELL OR CONFIG_BROADWELL)
	add_subdirectory(haswell)
elseif(CONFIG_CAVS)
	add_subdirectory(cavs)
elseif(CONFIG_ACE)
	add_subdirectory(ace)
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
