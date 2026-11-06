# SPDX-License-Identifier: BSD-3-Clause

is_zephyr(zephyr)
if(zephyr) ###  Zephyr ###

add_subdirectory(telemetry)

zephyr_library_sources_ifdef(CONFIG_COLD_STORE_EXECUTE_DEBUG dram.c)

endif() # Zephyr
