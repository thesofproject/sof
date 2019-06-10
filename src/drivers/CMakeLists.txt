# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_IMX8)
	add_subdirectory(imx)
else()
	add_subdirectory(intel)
	add_subdirectory(dw)
endif()
