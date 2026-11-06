# SPDX-License-Identifier: BSD-3-Clause

# Files common to Zephyr and XTOS
add_local_sources(sof
		handler.c
		helper.c
		dai.c
)

is_zephyr(zephyr)
if(zephyr) ###  Zephyr ###

zephyr_library_sources(
	host-page-table.c
)

endif() # Zephyr
