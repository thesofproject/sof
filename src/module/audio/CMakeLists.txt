# SPDX-License-Identifier: BSD-3-Clause

is_zephyr(it_is)
if(it_is)  ### Zephyr ###
	zephyr_library_sources(
		source_api.c
		sink_api.c
	)
else()  ### Not Zephyr ###
	add_local_sources(sof
		source_api.c
		sink_api.c
	)
endif() # Zephyr
