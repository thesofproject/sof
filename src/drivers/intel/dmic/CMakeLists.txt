# SPDX-License-Identifier: BSD-3-Clause

add_local_sources(sof dmic.c)
if(CONFIG_INTEL_DMIC_TPLG_PARAMS)
    add_local_sources(sof dmic_computed.c)
endif()

if(CONFIG_INTEL_DMIC_NHLT)
    add_local_sources(sof dmic_nhlt.c)
endif()
