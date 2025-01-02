# SPDX-License-Identifier: BSD-3-Clause

add_subdirectory(afe)
if(CONFIG_MT8195)
	add_subdirectory(mt8195)
endif()
if(CONFIG_MT8186 OR CONFIG_MT8188)
        add_subdirectory(mt818x)
endif()
if(CONFIG_MT8196)
	add_subdirectory(mt8196)
endif()
