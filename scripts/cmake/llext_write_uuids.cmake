# SPDX-License-Identifier: BSD-3-Clause

# Used by LLEXT modules to create a file with module UUIDs
file(STRINGS ${ZEPHYR_BINARY_DIR}/${MODULE}_llext/rimage_config.toml uuids REGEX "^[ \t]*uuid *=")

set(UUIDS_LIST_FILE ${ZEPHYR_BINARY_DIR}/${MODULE}_llext/llext.uuid)
file(REMOVE ${UUIDS_LIST_FILE})
foreach(line IN LISTS uuids)
	# extract UUID value - drop the 'uuid = ' part of the assignment line
	string(REGEX REPLACE "^[ \t]*uuid *= \"([0-9A-Fa-f\\-]*)\"" "\\1" uuid ${line})
	string(TOUPPER ${uuid} uuid)
	file(APPEND ${UUIDS_LIST_FILE} "${uuid}\n")
endforeach()
