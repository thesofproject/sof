file(GLOB TPLG_DEPS
	../../topology/platform/intel/*.m4
	../../topology/common/*.m4
	../../topology/m4/*.m4
	../../topology/sof/*.m4
)

find_program(XARGS NAMES xargs PATHS ENV PATH NO_DEFAULT_PATH)

if(${XARGS} STREQUAL XARGS-NOTFOUND)
	set(USE_XARGS "no")
else()
	set(USE_XARGS "yes")
endif()

add_custom_target(tests
	COMMAND ${CMAKE_COMMAND} -E env
		SOF_TPLG_BUILD_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}
		USE_XARGS=${USE_XARGS}
		./tplg-build.sh
	DEPENDS ${TPLG_DEPS}
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	VERBATIM
	USES_TERMINAL
)

add_custom_target(tests-clean
	COMMAND rm -f ${CMAKE_CURRENT_BINARY_DIR}/*.conf
	COMMAND rm -f ${CMAKE_CURRENT_BINARY_DIR}/*.tplg
	VERBATIM
	USES_TERMINAL
)
