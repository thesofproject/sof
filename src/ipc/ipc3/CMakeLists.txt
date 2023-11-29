# SPDX-License-Identifier: BSD-3-Clause

# Files common to Zephyr and XTOS
add_local_sources(sof
		handler.c
		helper.c
		dai.c
)

is_zephyr(it_is)
if(it_is) ###  Zephyr ###

zephyr_library_sources(
	host-page-table.c
)

else()  ### Not Zephyr ###

if (CONFIG_HOST_PTABLE)
	add_local_sources(sof
		host-page-table.c)
endif()

endif() # Zephyr
