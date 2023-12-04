# This file is intended to be included from project's CMakeLists.txt.
# Prior to include, MODULES_LIST variable should be initialised with list
# of modules (subdirectories in modules dir) that should be built into
# project's loadable library.

if(NOT DEFINED MODULES_LIST)
	message(FATAL_ERROR "Please define MODULES_LIST: list of modules to be built into loadable library")
endif()

include(${CMAKE_CURRENT_LIST_DIR}/config.cmake)

# Build common module functions from sof to a static library
add_library(sof STATIC)
target_include_directories(sof PRIVATE "${SOF_BASE}/src/include")
add_subdirectory("${SOF_BASE}/src/module" module_api)

foreach(MODULE ${MODULES_LIST})
	add_executable(${MODULE})
	add_subdirectory(${LMDK_BASE}/modules/${MODULE} ${MODULE}_module)

	# uncomment line below to compile module with debug information
	#target_compile_options(${MODULE} PUBLIC  "-g3")

	target_include_directories(${MODULE} PRIVATE
		"${LMDK_BASE}/include"
		"${RIMAGE_INCLUDE_DIR}"
		"${SOF_BASE}/src/include/module"
	)

	# generate linker script
	get_target_property(HPSRAM_ADDR ${MODULE} HPSRAM_ADDR)

	if(NOT DEFINED HPSRAM_ADDR)
		message(FATAL_ERROR "Please define HPSRAM_ADDR for module ${MODULE}")
	endif()

	add_custom_command(TARGET ${MODULE} PRE_LINK
		COMMAND ${CMAKE_COMMAND}
		-DMODULE=${MODULE}
		-DHPSRAM_ADDR=${HPSRAM_ADDR}
		-P ${CMAKE_CURRENT_LIST_DIR}/ldscripts.cmake
	)

	# Link module with sof common module functions
	target_link_libraries(${MODULE} sof)

	target_link_options(${MODULE} PRIVATE
		"-nostartfiles"
		"-Wl,--no-undefined" "-Wl,--unresolved-symbols=report-all" "-Wl,--error-unresolved-symbols"
		"-Wl,--gc-sections"
		"-Wl,-Map,$<TARGET_FILE:${MODULE}>.map"	# optional: just for debug
		"-T" "${MODULE}_ldscripts/elf32xtensa.x"
	)
endforeach()

set(RIMAGE_OUTPUT_FILE ${PROJECT_NAME}_noextmft)
set(OUTPUT_FILE ${PROJECT_NAME}.bin)

if(RIMAGE_INSTALL_DIR)
  cmake_path(ABSOLUTE_PATH RIMAGE_INSTALL_DIR BASE_DIRECTORY ${CMAKE_SOURCE_DIR} NORMALIZE)
endif()

# Create a hint - rimage may be installed to directory where SOF project installs it
cmake_path(APPEND SOF_BASE "../build-rimage" OUTPUT_VARIABLE RIMAGE_SOF_INSTALL_DIR)
cmake_path(NORMAL_PATH RIMAGE_SOF_INSTALL_DIR)
cmake_path(ABSOLUTE_PATH SIGNING_KEY BASE_DIRECTORY ${CMAKE_SOURCE_DIR} NORMALIZE)

foreach(MOD_NAME IN LISTS MODULES_LIST)
	list(APPEND RIMAGE_MODULES_LIST ${MOD_NAME}.mod)

	# Change .module section flags to tell rimage to not include it in a final image
	add_custom_target(${MOD_NAME}.mod
		COMMENT "Preparing .mod(ule) files for rimage"
		DEPENDS ${MOD_NAME}
		COMMAND ${CMAKE_OBJCOPY}
			--set-section-flags .module=noload,readonly
			${MOD_NAME} ${MOD_NAME}.mod
)
endforeach()

find_program(RIMAGE_COMMAND NAMES rimage
  PATHS "${RIMAGE_INSTALL_DIR}"
  HINTS "${RIMAGE_SOF_INSTALL_DIR}"
  REQUIRED)

add_custom_target(${PROJECT_NAME}_target ALL
	DEPENDS ${RIMAGE_MODULES_LIST}
	COMMAND ${RIMAGE_COMMAND} -l -k ${SIGNING_KEY} -f 2.0.0 -b 1 -o ${RIMAGE_OUTPUT_FILE} -c ${TOML} -e ${RIMAGE_MODULES_LIST}
	COMMAND ${CMAKE_COMMAND} -E cat ${RIMAGE_OUTPUT_FILE}.xman ${RIMAGE_OUTPUT_FILE} > ${OUTPUT_FILE}
)
