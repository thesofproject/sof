# SPDX-License-Identifier: BSD-3-Clause

# platform-specific values

if(CONFIG_BAYTRAIL)
	set(platform_folder baytrail)
elseif(CONFIG_CHERRYTRAIL)
	set(platform_folder baytrail)
elseif(CONFIG_HASWELL)
	set(platform_folder haswell)
elseif(CONFIG_BROADWELL)
	set(platform_folder haswell)
elseif(CONFIG_APOLLOLAKE)
	set(platform_folder apollolake)
elseif(CONFIG_CANNONLAKE)
	set(platform_folder cannonlake)
elseif(CONFIG_SUECREEK)
	set(platform_folder suecreek)
elseif(CONFIG_ICELAKE)
	set(platform_folder icelake)
elseif(CONFIG_TIGERLAKE)
	set(platform_folder tigerlake)
	if(XCC)
		set(RIMAGE_MOD_OFFSET_FLAG -x 24)
	endif()
elseif(CONFIG_IMX8)
	set(platform_folder imx8)
elseif(CONFIG_IMX8X)
	set(platform_folder imx8)
elseif(CONFIG_IMX8M)
	set(platform_folder imx8m)
endif()

if(CONFIG_CAVS)
	set(family_path intel/cavs)
endif()

set(fw_name ${CONFIG_RIMAGE_SIGNING_SCHEMA})

if(CONFIG_BOOT_LOADER)
	set(build_bootloader y)
	set(build_module y)
endif()

set(platform_ld_script ${platform_folder}.x)
set(platform_rom_ld_script rom.x)
set(platform_bootldr_ld_script boot_ldr.x)

# includes

target_include_directories(sof_options INTERFACE
	${PROJECT_SOURCE_DIR}/src/arch/xtensa/include
	${PROJECT_SOURCE_DIR}/src/arch/xtensa/xtos
)

target_include_directories(sof_options INTERFACE ${PROJECT_SOURCE_DIR}/src/platform/${platform_folder}/include)

if(XCC)
	target_include_directories(sof_options INTERFACE ${ROOT_DIR}/arch/include)
else()
	target_include_directories(sof_options INTERFACE ${PROJECT_SOURCE_DIR}/src/platform/${platform_folder}/include/arch)
endif()

target_include_directories(sof_options INTERFACE ${ROOT_DIR}/include)

if(BUILD_UNIT_TESTS)
	set(stdlib_flag "")
else()
	set(stdlib_flag "-nostdlib")
endif()

get_optimization_flag(optimization_flag)

if(BUILD_CLANG_SCAN)
	# pretend to be xtensa compiler to go trough the same paths for AST
	target_compile_definitions(sof_options INTERFACE -D__XTENSA__=1)

	if(XCC)
		# clang has to compile objects in order to analyze sources,
		# so it needs xcc's headers
		find_program(XCC_PATH NAMES "xt-xcc" PATHS ENV PATH NO_DEFAULT_PATH)
		get_filename_component(XCC_DIR ${XCC_PATH} DIRECTORY)
		target_include_directories(sof_options INTERFACE ${XCC_DIR}/../xtensa-elf/include)
	endif()

	# Clang by default compiles for host architecture,
	# but xtensa is always 32 bit, what may cause mismatch in definitions,
	# that depend on bitness, so force compilation for 32 bit.
	set(XTENSA_C_ASM_FLAGS -m32)
	set(XTENSA_C_FLAGS)
else()
	set(XTENSA_C_ASM_FLAGS -mlongcalls)
	set(XTENSA_C_FLAGS -mtext-section-literals)
endif()

# linker flags - GCC >= 10.x uses libc
if (CMAKE_COMPILER_IS_GNUCC AND CMAKE_C_COMPILER_VERSION VERSION_GREATER 10.0)
	target_link_libraries(sof_options INTERFACE ${stdlib_flag} -lgcc -lc -Wl,--no-check-sections -ucall_user_start -Wl,-static)
else()
	target_link_libraries(sof_options INTERFACE ${stdlib_flag} -lgcc -Wl,--no-check-sections -ucall_user_start -Wl,-static)
endif()

# C & ASM flags
target_compile_options(sof_options INTERFACE ${stdlib_flag} -fno-inline-functions ${XTENSA_C_ASM_FLAGS})

# C flags
# TODO: Generator expressions are supported only with Make and Ninja,
# if we want to support other generators, we would have to find some other way
# for setting flags just for C files.
# Possible solutions:
#   1) CMAKE_<LANG>_FLAGS - works, but is global, we prefer target_* functions
#   2) set_source_files_properties - need to be done for each source file, it's
#      better to have set of default flags and change it only for special cases
#   3) custom function that is used instead of target_sources and sets flags
#      for each added source based on file extension

# No space between -imacros and its argument to avoid CMake
# de-duplication "feature"
target_compile_options(sof_options INTERFACE
	$<$<COMPILE_LANGUAGE:C>:
		-${optimization_flag} -g
		-Wall -Werror
		-Wl,-EL
		-Wmissing-prototypes
		-Wpointer-arith
		${XTENSA_C_FLAGS}
	>
	-imacros${CONFIG_H_PATH}
	)

if(BUILD_UNIT_TESTS)
	# rest of this file is not needed for unit tests
	return()
endif()

if(XCC)
	file(GLOB LINK_DEPS
		${ROOT_DIR}/arch/include/xtensa/config/core-isa*)
else()
	file(GLOB LINK_DEPS
		${PROJECT_SOURCE_DIR}/src/platform/${platform_folder}/include/arch/xtensa/config/core-isa*)
endif()

# linker scripts

function(sof_add_ld_script binary_name script_name)

	set(lds_in ${PROJECT_SOURCE_DIR}/src/platform/${platform_folder}/${script_name}.in)
	set(lds_out ${PROJECT_BINARY_DIR}/${script_name})

	get_target_property(incdirs sof_options INTERFACE_INCLUDE_DIRECTORIES)

	set(iflags "")
	set(glob_predicates "")
	foreach(d ${incdirs})
		list(APPEND iflags "-I${d}")
		list(APPEND glob_predicates "${d}/*.h")
	endforeach()

	get_target_property(incdirs sof_public_headers INTERFACE_INCLUDE_DIRECTORIES)

	foreach(d ${incdirs})
		list(APPEND iflags "-I${d}")
		list(APPEND glob_predicates "${d}/*.h")
	endforeach()

	file(GLOB lds_headers ${glob_predicates})

	add_custom_command(OUTPUT ${lds_out}
		COMMAND ${CMAKE_C_COMPILER} -E -DLINKER -P ${iflags} -o ${lds_out} -x c ${lds_in}
			-imacros${CONFIG_H_PATH}
		DEPENDS ${lds_in} ${LINK_DEPS} genconfig ${CONFIG_H_PATH} ${lds_headers}
		WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
		COMMENT "Generating linker script: ${lds_out}"
		VERBATIM
		USES_TERMINAL
	)

	add_custom_target("ld_script_${script_name}" DEPENDS ${lds_out})
	add_dependencies(${binary_name} "ld_script_${script_name}")
	set_target_properties(${binary_name} PROPERTIES LINK_DEPENDS ${lds_in})
endfunction()

sof_add_ld_script(sof ${platform_ld_script})

# binaries

add_library(reset STATIC "")
target_link_libraries(reset sof_options)
target_compile_options(reset PRIVATE -mtext-section-literals)

add_subdirectory(debug)
add_subdirectory(drivers)
add_subdirectory(hal)
add_subdirectory(lib)
add_subdirectory(schedule)
add_subdirectory(xtos)

add_local_sources(reset xtos/memctl_default.S xtos/reset-vector.S)

add_local_sources(sof
	xtos/crt1-boards.S
	xtos/_vectors.S
	init.c
	exc-dump.S
)

# TODO: order of these libraries does matter, what is bad,
# we should switch to building with thin archives without symbols index
# and made it before final link so dependencies won't matter
target_link_libraries(sof_static_libraries INTERFACE xtos)
target_link_libraries(sof_static_libraries INTERFACE hal)

if(CONFIG_INTERRUPT_LEVEL_2)
target_link_libraries(sof_static_libraries INTERFACE xlevel2)
endif()
if(CONFIG_INTERRUPT_LEVEL_3)
target_link_libraries(sof_static_libraries INTERFACE xlevel3)
endif()
if(CONFIG_INTERRUPT_LEVEL_4)
target_link_libraries(sof_static_libraries INTERFACE xlevel4)
endif()
if(CONFIG_INTERRUPT_LEVEL_5)
target_link_libraries(sof_static_libraries INTERFACE xlevel5)
endif()

if(build_bootloader)
	add_local_sources(sof main-entry.S)
else()
	target_link_libraries(sof_static_libraries INTERFACE reset)
endif()

target_link_libraries(sof_ld_flags INTERFACE "-Wl,-Map=sof.map")
target_link_libraries(sof_ld_flags INTERFACE "-T${PROJECT_BINARY_DIR}/${platform_ld_script}")

add_custom_target(
	prepare_sof_post_process
	DEPENDS sof
	COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_BINARY_DIR}/sof sof-${fw_name}
)

# contains steps that should be performed before fw image is ready for being
# processed by tools like rimage and MEU
add_custom_target(sof_post_process DEPENDS prepare_sof_post_process)

# contains extra output that should be generated for bin target
add_custom_target(bin_extras)

# bootloader binary

if(build_bootloader)
	add_executable(bootloader "")
	target_link_libraries(bootloader PRIVATE sof_options)
	add_local_sources(bootloader xtos/_vectors.S ${PROJECT_SOURCE_DIR}/src/platform/${family_path}/boot_entry.S ${PROJECT_SOURCE_DIR}/src/platform/${family_path}/boot_loader.c)
	target_link_libraries(bootloader PRIVATE reset)
	target_link_libraries(bootloader PRIVATE hal)
	target_link_libraries(bootloader PRIVATE "-T${PROJECT_BINARY_DIR}/${platform_bootldr_ld_script}")
	sof_add_ld_script(bootloader ${platform_bootldr_ld_script})
	sof_append_relative_path_definitions(bootloader)

	add_custom_target(
		bootloader_dump
		COMMAND ${CMAKE_COMMAND} -E copy bootloader bootloader-${fw_name}
		COMMAND ${CMAKE_OBJCOPY} -O binary ${PROJECT_BINARY_DIR}/src/platform/${platform_folder}/boot_module mod-boot-${fw_name}.bin
		COMMAND ${CMAKE_OBJCOPY} --add-section .module=mod-boot-${fw_name}.bin --set-section-flags .module=load,readonly bootloader-${fw_name}
		COMMAND ${CMAKE_OBJCOPY} -O binary bootloader bootloader-${fw_name}.bin
		COMMAND ${CMAKE_OBJDUMP} -h -D bootloader > bootloader-${fw_name}.lmap
		COMMAND ${CMAKE_OBJDUMP} -S bootloader > bootloader-${fw_name}.lst
		COMMAND ${CMAKE_OBJDUMP} -D bootloader > bootloader-${fw_name}.dis
		DEPENDS bootloader boot_module
		VERBATIM
		USES_TERMINAL
	)

	set(bootloader_binary_path bootloader-${fw_name})

	add_custom_target(
		process_base_module
		COMMAND ${CMAKE_OBJCOPY} -O binary ${PROJECT_BINARY_DIR}/src/platform/${platform_folder}/base_module mod-${fw_name}.bin
		COMMAND ${CMAKE_OBJCOPY} --add-section .module=mod-${fw_name}.bin --set-section-flags .module=load,readonly sof-${fw_name}
		DEPENDS prepare_sof_post_process base_module bootloader_dump
		VERBATIM
		USES_TERMINAL
	)

	add_dependencies(sof_post_process process_base_module)
else()
	set(bootloader_binary_path)
endif()

if(CONFIG_BUILD_VM_ROM)
	add_executable(rom "")
	target_link_libraries(rom PRIVATE sof_options)
	target_link_libraries(rom PRIVATE "-T${PROJECT_BINARY_DIR}/${platform_rom_ld_script}")
	sof_add_ld_script(rom ${platform_rom_ld_script})

	# We have to make additional define, because sources
	# are reused for other objects with different flags.
	target_compile_definitions(rom PRIVATE -DCONFIG_VM_ROM)

	add_local_sources(rom
		xtos/crt1-boards-rom.S
		xtos/memctl_default.S
		xtos/reset-vector.S
	)

	add_custom_target(
		rom_dump
		COMMAND ${CMAKE_COMMAND} -E copy rom rom-${fw_name}
		COMMAND ${CMAKE_OBJCOPY} -O binary rom rom-${fw_name}.bin
		COMMAND ${CMAKE_OBJDUMP} -h -D rom > rom-${fw_name}.lmap
		COMMAND ${CMAKE_OBJDUMP} -S rom > rom-${fw_name}.lst
		COMMAND ${CMAKE_OBJDUMP} -D rom > rom-${fw_name}.dis
		DEPENDS rom
		VERBATIM
		USES_TERMINAL
	)

	add_dependencies(bin_extras rom_dump)
endif()

if(BUILD_CLANG_SCAN)
	# steps below don't compile parts of fw,
	# so they are not needed for scan-build
	return()
endif()

add_custom_target(
	sof_dump
	COMMAND ${CMAKE_OBJDUMP} -S sof-${fw_name} > sof-${fw_name}.lst
	COMMAND ${CMAKE_OBJDUMP} -h sof-${fw_name} > sof-${fw_name}.lmap
	COMMAND ${CMAKE_OBJDUMP} -D sof-${fw_name} > sof-${fw_name}.dis
	DEPENDS sof_post_process
	VERBATIM
	USES_TERMINAL
)

include(ExternalProject)

# smex

ExternalProject_Add(smex_ep
	DEPENDS check_version_h
	DOWNLOAD_COMMAND ""
	SOURCE_DIR "${PROJECT_SOURCE_DIR}/smex"
	PREFIX "${PROJECT_BINARY_DIR}/smex_ep"
	BINARY_DIR "${PROJECT_BINARY_DIR}/smex_ep/build"
	BUILD_ALWAYS 1
	INSTALL_COMMAND ""
)

add_custom_target(
	run_smex
	COMMAND ${PROJECT_BINARY_DIR}/smex_ep/build/smex
		-l sof-${fw_name}.ldc
		sof-${fw_name}
	DEPENDS sof_dump smex_ep
	VERBATIM
	USES_TERMINAL
)

# rimage

ExternalProject_Add(rimage_ep
	DEPENDS check_version_h
	SOURCE_DIR "${PROJECT_SOURCE_DIR}/rimage"
	PREFIX "${PROJECT_BINARY_DIR}/rimage_ep"
	BINARY_DIR "${PROJECT_BINARY_DIR}/rimage_ep/build"
	BUILD_ALWAYS 1
	INSTALL_COMMAND ""
)

if(NOT DEFINED RIMAGE_PRIVATE_KEY)
	set(RIMAGE_PRIVATE_KEY ${PROJECT_SOURCE_DIR}/keys/otc_private_key.pem)
endif()

if(NOT DEFINED RIMAGE_IMR_TYPE)
	# default value for non-production firmware
	set(RIMAGE_IMR_TYPE 3)
endif()

if(NOT MEU_OPENSSL)
	set(MEU_OPENSSL "/usr/bin/openssl")
endif()
# Don't ask users to keep secret their openssl location depending on
# what they build in the moment.
set(silenceUnusedWarning "${MEU_OPENSSL}")

if(MEU_PATH OR DEFINED MEU_NO_SIGN) # Don't sign with rimage
	if(NOT DEFINED MEU_OFFSET)
		execute_process(
			COMMAND ${MEU_PATH}/meu -ver
			OUTPUT_VARIABLE MEU_VERSION_FULL_TEXT
			OUTPUT_STRIP_TRAILING_WHITESPACE
			RESULT_VARIABLE meu_ver_res
		)
		if(NOT ${meu_ver_res} EQUAL 0)
			message(WARNING "${MEU_PATH}/meu -ver"
			  " failed with: ${meu_ver_res}")
		endif()

		string(REGEX MATCH "Version:[\t\n ]*([^\t\n ]+)" ignored "${MEU_VERSION_FULL_TEXT}")
		set(MEU_VERSION ${CMAKE_MATCH_1})

		if(MEU_VERSION VERSION_LESS 12.0.0.1035)
			set(MEU_OFFSET 1152)
		elseif(MEU_VERSION VERSION_LESS 15.0.0.0)
			set(MEU_OFFSET 1088)
		else()
			set(MEU_OFFSET 1344)
		endif()
	endif()
	message(STATUS MEU_OFFSET=${MEU_OFFSET})

	# Passing -s ${MEU_OFFSET} disables rimage signing and produces
	# one .uns file and one .met file instead of a .ri file.
	add_custom_target(
		run_rimage
		COMMAND ${PROJECT_BINARY_DIR}/rimage_ep/build/rimage
			-o sof-${fw_name}.ri
			-c "${PROJECT_SOURCE_DIR}/rimage/config/${fw_name}.toml"
			-s ${MEU_OFFSET}
			-k ${RIMAGE_PRIVATE_KEY}
			-i ${RIMAGE_IMR_TYPE}
			-f ${SOF_MAJOR}.${SOF_MINOR}
			-b ${SOF_BUILD}
			-e
			${RIMAGE_MOD_OFFSET_FLAG}
			${bootloader_binary_path}
			sof-${fw_name}
		DEPENDS sof_dump rimage_ep
		VERBATIM
		USES_TERMINAL
	)

	if(CONFIG_TRACE)
		add_dependencies(run_rimage run_smex)
	endif()

	if(NOT DEFINED MEU_FLAGS)
		set(MEU_FLAGS
			-f ${MEU_PATH}/generic_meu_conf.xml
			-mnver 0.0.0.0
			-key ${MEU_PRIVATE_KEY}
			-stp ${MEU_OPENSSL}
			${MEU_EXTRA_FLAGS}
		)
	endif()

	if(MEU_NO_SIGN)
		add_custom_target(run_meu DEPENDS run_rimage)
	else()
		add_custom_target(
			run_meu
			COMMAND ${MEU_PATH}/meu -w ./ -s sof-${fw_name}
				${MEU_FLAGS}
				-o sof-${fw_name}.ri
			DEPENDS run_rimage
			VERBATIM
			USES_TERMINAL
		)
	endif()
else() # sign with rimage
	add_custom_target(
		run_rimage
		COMMAND ${PROJECT_BINARY_DIR}/rimage_ep/build/rimage
			-o sof-${fw_name}.ri
			-c "${PROJECT_SOURCE_DIR}/rimage/config/${fw_name}.toml"
			-k ${RIMAGE_PRIVATE_KEY}
			-i ${RIMAGE_IMR_TYPE}
			-f ${SOF_MAJOR}.${SOF_MINOR}
			-b ${SOF_BUILD}
			-e
			${RIMAGE_MOD_OFFSET_FLAG}
			${bootloader_binary_path}
			sof-${fw_name}
		DEPENDS sof_dump rimage_ep
		VERBATIM
		USES_TERMINAL
	)

	if (CONFIG_TRACE)
		add_dependencies(run_rimage run_smex)
	endif()

	add_custom_target(run_meu DEPENDS run_rimage)
endif() # sign with MEU / nothing / rimage

if(NOT DEFINED FIRMWARE_NAME)
	set(fw_output_name "${fw_name}")
else()
	set(fw_output_name "${FIRMWARE_NAME}")
endif()

if(${CMAKE_HOST_WIN32})
	set(GLUE_CMD copy /b sof-${fw_name}.ri.xman + sof-${fw_name}.ri sof-${fw_name}.rix)
else()
	set(GLUE_CMD cat sof-${fw_name}.ri.xman sof-${fw_name}.ri > sof-${fw_name}.rix)
endif()

add_custom_target(
	glue_binary_files
	COMMAND ${GLUE_CMD}
	COMMAND ${CMAKE_COMMAND} -E remove sof-${fw_name}.ri
	COMMAND ${CMAKE_COMMAND} -E rename sof-${fw_name}.rix sof-${fw_name}.ri
	DEPENDS run_meu
	VERBATIM
	USES_TERMINAL
)

if(MEU_NO_SIGN)
	# copy rimage output that can be used to sign firmware
	add_custom_target(
		bin ALL
		COMMAND ${CMAKE_COMMAND} -E copy sof-${fw_name}.ri.uns ${PROJECT_BINARY_DIR}/sof-${fw_output_name}.ri.uns
		COMMAND ${CMAKE_COMMAND} -E copy sof-${fw_name}.ri.met ${PROJECT_BINARY_DIR}/sof-${fw_output_name}.ri.met
		COMMAND ${CMAKE_COMMAND} -E copy sof-${fw_name}.ldc ${PROJECT_BINARY_DIR}/sof-${fw_output_name}.ldc
		DEPENDS run_meu bin_extras glue_binary_files
		VERBATIM
		USES_TERMINAL
	)
else()
	add_custom_target(
		bin ALL
		COMMAND ${CMAKE_COMMAND} -E copy sof-${fw_name}.ri ${PROJECT_BINARY_DIR}/sof-${fw_output_name}.ri
		COMMAND ${CMAKE_COMMAND} -E copy sof-${fw_name}.ri ${PROJECT_BINARY_DIR}/sof.ri
		COMMAND ${CMAKE_COMMAND} -E copy sof-${fw_name}.ldc ${PROJECT_BINARY_DIR}/sof-${fw_output_name}.ldc
		COMMAND ${CMAKE_COMMAND} -E copy sof-${fw_name}.ldc ${PROJECT_BINARY_DIR}/sof.ldc
		DEPENDS run_meu bin_extras glue_binary_files
		VERBATIM
		USES_TERMINAL
	)
endif()

install(
	FILES ${PROJECT_BINARY_DIR}/sof-${fw_output_name}.ri
		${PROJECT_BINARY_DIR}/sof-${fw_output_name}.ldc
	DESTINATION bin
)
