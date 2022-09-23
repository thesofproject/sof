# SPDX-License-Identifier: BSD-3-Clause
add_custom_target(topologies2)

# Check alsatplg version and build topology2 if alsatplg version is
# 1.2.7 or greater, see https://github.com/thesofproject/sof/issues/5323
execute_process(COMMAND alsatplg --version RESULT_VARIABLE STATUS OUTPUT_VARIABLE ALSA_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
if(STATUS AND NOT STATUS EQUAL 0)
	message(WARNING "alsatplg error: ${STATUS}, topology2 will be skipped")
else()
	string(REPLACE "\n" ";" ALSA_VERSION_LIST ${ALSA_VERSION})
	list(GET ALSA_VERSION_LIST 0 ALSATPLG_VERSION)
	string(REGEX MATCH "[0-9]\.[0-9]\.*[0-9]*" ALSATPLG_VERSION_NUMBER ${ALSATPLG_VERSION})
	if(${ALSATPLG_VERSION_NUMBER} VERSION_LESS "1.2.7")
		message(WARNING "topology2 will be skipped. Minimum required version for alsatplg: 1.2.7")
	else()
		add_dependencies(topologies topologies2)
	endif()
endif()

add_subdirectory(cavs)
