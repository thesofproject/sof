# SPDX-License-Identifier: BSD-3-Clause

is_zephyr(zephyr)
if(zephyr)  ### Zephyr ###

zephyr_library_sources(
	init.c
	ext_manifest.c
)

endif() # Zephyr
