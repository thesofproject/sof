# SPDX-License-Identifier: BSD-3-Clause

add_local_sources(sof
		handler.c
		helper.c
		dai.c
)

if (CONFIG_HOST_PTABLE)
	add_local_sources(sof
		host-page-table.c)
endif()