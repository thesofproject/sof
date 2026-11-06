# SPDX-License-Identifier: BSD-3-Clause

add_subdirectory(tester)

is_zephyr(zephyr)
if(zephyr) ###  Zephyr ###

add_subdirectory(debug_stream)

endif() # Zephyr
