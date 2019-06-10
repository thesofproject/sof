# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_IMX8)
	add_subdirectory(imx)
else()
	add_subdirectory(intel)
endif()

if(CONFIG_DW)
	add_subdirectory(dw)
endif()
