# SPDX-License-Identifier: BSD-3-Clause

set(common_files
		handler.c
		helper.c
		dai.c
)

is_zephyr(it_is)
if(it_is) ###  Zephyr ###

zephyr_library_sources(
	${common_files}
	host-page-table.c
)

else()  ### Not Zephyr ###

add_local_sources(sof
	${common_files}
)

if (CONFIG_HOST_PTABLE)
	add_local_sources(sof
		host-page-table.c)
endif()

endif() # Zephyr
