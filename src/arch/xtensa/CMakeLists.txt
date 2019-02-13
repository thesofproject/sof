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
endif()

set(fw_name ${CONFIG_FIRMWARE_SHORT_NAME})

if(CONFIG_APOLLOLAKE
   OR CONFIG_CANNONLAKE
   OR CONFIG_SUECREEK
   OR CONFIG_ICELAKE)
	set(build_bootloader y)
	set(build_module y)
endif()

set(platform_ld_script ${platform_folder}.x)
set(platform_rom_ld_script rom.x)
set(platform_bootldr_ld_script boot_ldr.x)

# includes

target_include_directories(sof_options INTERFACE ${PROJECT_SOURCE_DIR}/src/arch/xtensa/include)

if(CONFIG_SMP)
	target_include_directories(sof_options INTERFACE
		${PROJECT_SOURCE_DIR}/src/arch/xtensa/smp/include
		${PROJECT_SOURCE_DIR}/src/arch/xtensa/smp/xtos
	)
else()
	target_include_directories(sof_options INTERFACE
		${PROJECT_SOURCE_DIR}/src/arch/xtensa/up/include
		${PROJECT_SOURCE_DIR}/src/arch/xtensa/up/xtos
	)
endif()

target_include_directories(sof_options INTERFACE ${PROJECT_SOURCE_DIR}/src/platform/${platform_folder}/include)

if(XCC)
	target_include_directories(sof_options INTERFACE ${ROOT_DIR}/arch/include)
else()
	target_include_directories(sof_options INTERFACE ${PROJECT_SOURCE_DIR}/src/platform/${platform_folder}/include/arch)
endif()

target_include_directories(sof_options INTERFACE ${ROOT_DIR}/include)

if(BUILD_UNIT_TESTS)
	# rest of this file is not needed for unit tests
	return()
endif()

# linker flags
target_link_libraries(sof_options INTERFACE -nostdlib -Wl,--no-check-sections -ucall_user_start -Wl,-static)

# C & ASM flags
target_compile_options(sof_options INTERFACE -fno-inline-functions -nostdlib -mlongcalls)

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
target_compile_options(sof_options INTERFACE $<$<COMPILE_LANGUAGE:C>:-O2 -g -Wall -Werror -Wl,-EL -Wmissing-prototypes -mtext-section-literals>)

if(XCC)
	file(GLOB LINK_DEPS
		${ROOT_DIR}/arch/include/xtensa/config/core-isa*)
else()
	file(GLOB LINK_DEPS
		${PROJECT_SOURCE_DIR}/src/platform/${platform_folder}/include/arch/xtensa/config/core-isa*)
endif()

# linker scripts

function(sof_add_ld_script binary_name script_name)
	if(NOT EXISTS ${DOT_CONFIG_PATH})
		return()
	endif()

	set(lds_in ${PROJECT_SOURCE_DIR}/src/platform/${platform_folder}/${script_name}.in)
	set(lds_out ${PROJECT_BINARY_DIR}/${script_name})

	get_target_property(incdirs sof_options INTERFACE_INCLUDE_DIRECTORIES)

	set(iflags "")
	foreach(d ${incdirs})
		list(APPEND iflags "-I${d}")
	endforeach()

	add_custom_command(OUTPUT ${lds_out}
		COMMAND ${CMAKE_C_COMPILER} -E -P ${iflags} -o ${lds_out} -x c ${lds_in}
		DEPENDS ${lds_in} ${LINK_DEPS} genconfig ${CONFIG_H_PATH}
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

if(CONFIG_SMP)
	add_local_sources(reset
		smp/xtos/memctl_default.S
		smp/xtos/reset-vector.S
	)
else()
	add_local_sources(reset
		up/xtos/reset-vector.S
	)
endif()

add_local_sources(sof timer.c)

if(CONFIG_GDB_DEBUG)
	add_subdirectory(gdb)
endif()

if(CONFIG_SMP)
# TODO it should be handled in subdir
	add_subdirectory(smp)
	add_local_sources(sof
		smp/xtos/crt1-boards.S
		smp/xtos/_vectors.S
		smp/cpu.c
		smp/idc.c
		smp/init.c
		smp/notifier.c
		smp/schedule.c
		smp/task.c
		smp/work.c
	)
else()
	add_subdirectory(up)
	add_local_sources(sof
		up/xtos/crt1-boards.S
		up/xtos/_vectors.S
		up/cpu.c
		up/init.c
		up/notifier.c
		up/schedule.c
		up/task.c
		up/work.c
	)
endif()

add_library(sof_lib INTERFACE)
target_link_libraries(sof_lib INTERFACE sof_options)

# TODO: order of these libraries does matter, what is bad,
# we should switch to building with thin archives without symbols index
# and made it before final link so dependencies won't matter
target_link_libraries(sof_lib INTERFACE xtos)
target_link_libraries(sof_lib INTERFACE hal)

target_link_libraries(sof_lib INTERFACE xlevel2)
target_link_libraries(sof_lib INTERFACE xlevel3)
target_link_libraries(sof_lib INTERFACE xlevel4)
target_link_libraries(sof_lib INTERFACE xlevel5)

if(build_bootloader)
	add_local_sources(sof main-entry.S)
else()
	target_link_libraries(sof_lib INTERFACE reset)
endif()

target_link_libraries(sof_ld_flags INTERFACE "-lgcc")
target_link_libraries(sof_ld_flags INTERFACE "-Wl,-Map=sof.map")
target_link_libraries(sof_ld_flags INTERFACE "-T${PROJECT_BINARY_DIR}/${platform_ld_script}")

# rimage

include(ExternalProject)

ExternalProject_Add(rimage_ep
	DEPENDS check_version_h
	DOWNLOAD_COMMAND ""
	SOURCE_DIR "${PROJECT_SOURCE_DIR}/rimage"
	PREFIX "${PROJECT_BINARY_DIR}/rimage_ep"
	BINARY_DIR "${PROJECT_BINARY_DIR}/rimage_ep/build"
	BUILD_ALWAYS 1
	CMAKE_ARGS -DVERSION_H_PATH=${VERSION_H_PATH}
		-DPEM_KEY_PREFIX=${PROJECT_SOURCE_DIR}/rimage/keys
	INSTALL_COMMAND ""
)

add_custom_target(
	prepare_fw_copy_for_rimage
	DEPENDS sof rimage_ep
	COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_BINARY_DIR}/sof sof-${fw_name}
)

# bootloader binary

if(build_bootloader)
	add_executable(bootloader "")
	target_link_libraries(bootloader PRIVATE sof_options)
	add_local_sources(bootloader boot_entry.S boot_loader.c)
	target_link_libraries(bootloader PRIVATE reset)
	target_link_libraries(bootloader PRIVATE hal)
	target_link_libraries(bootloader PRIVATE "-lgcc")
	target_link_libraries(bootloader PRIVATE "-T${PROJECT_BINARY_DIR}/${platform_bootldr_ld_script}")
	sof_add_ld_script(bootloader ${platform_bootldr_ld_script})

	if(CONFIG_SMP)
		add_local_sources(bootloader smp/xtos/_vectors.S)
	else()
		add_local_sources(bootloader up/xtos/_vectors.S)
	endif()

	add_custom_target(
		bootloader_dump
		COMMAND ${CMAKE_COMMAND} -E copy bootloader bootloader-${fw_name}
		COMMAND ${CMAKE_OBJCOPY} -O binary ${PROJECT_BINARY_DIR}/src/platform/${platform_folder}/boot_module mod-boot-${fw_name}.bin
		COMMAND ${CMAKE_OBJCOPY} --add-section .module=mod-boot-${fw_name}.bin --set-section-flags .module=load,readonly bootloader-${fw_name}
		COMMAND ${CMAKE_OBJCOPY} -O binary bootloader bootloader-${fw_name}.bin
		COMMAND ${CMAKE_OBJDUMP} -h -D bootloader > bootloader-${fw_name}.map
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
		DEPENDS prepare_fw_copy_for_rimage base_module bootloader_dump
		VERBATIM
		USES_TERMINAL
	)
else()
	set(bootloader_binary_path "")

	add_custom_target(process_base_module DEPENDS prepare_fw_copy_for_rimage)
endif()

add_custom_target(
	sof_dump
	COMMAND ${CMAKE_OBJDUMP} -S sof-${fw_name} > sof-${fw_name}.lst
	COMMAND ${CMAKE_OBJDUMP} -D sof-${fw_name} > sof-${fw_name}.dis
	DEPENDS process_base_module
	VERBATIM
	USES_TERMINAL
)

if(MEU_PATH)
	execute_process(
		COMMAND ${MEU_PATH}/meu -ver
		OUTPUT_VARIABLE MEU_VERSION_FULL_TEXT
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)

	string(REGEX MATCH "Version:[\t\n ]*([^\t\n ]+)" ignored "${MEU_VERSION_FULL_TEXT}")
	set(MEU_VERSION ${CMAKE_MATCH_1})

	if(MEU_VERSION VERSION_LESS 12.0.0.1035)
		set(MEU_OFFSET 1152)
	else()
		set(MEU_OFFSET 1088)
	endif()

	set(otc_private_key ${PROJECT_SOURCE_DIR}/rimage/keys/otc_private_key.pem)

	add_custom_target(
		run_rimage
		COMMAND ${PROJECT_BINARY_DIR}/rimage_ep/build/rimage
			-o sof-${fw_name}.ri
			-p sof-${fw_name}.ldc
			-m ${fw_name}
			-s ${MEU_OFFSET}
			-k ${otc_private_key}
			${bootloader_binary_path}
			sof-${fw_name}
		DEPENDS sof_dump
		VERBATIM
		USES_TERMINAL
	)

	if(NOT DEFINED MEU_OPENSSL)
		set(MEU_OPENSSL "/usr/bin/openssl")
	endif()

	add_custom_target(
		run_meu
		COMMAND ${MEU_PATH}/meu -w ./ -s sof-${fw_name}
			-key ${MEU_PRIVATE_KEY}
			-stp ${MEU_OPENSSL}
			-f ${MEU_PATH}/generic_meu_conf.xml
			-mnver 0.0.0.0 -o sof-${fw_name}.ri
		DEPENDS run_rimage
		VERBATIM
		USES_TERMINAL
	)
else()
	add_custom_target(
		run_rimage
		COMMAND ${PROJECT_BINARY_DIR}/rimage_ep/build/rimage
			-o sof-${fw_name}.ri
			-p sof-${fw_name}.ldc
			-m ${fw_name}
			${bootloader_binary_path}
			sof-${fw_name}
		DEPENDS sof_dump
		VERBATIM
		USES_TERMINAL
	)
	add_custom_target(run_meu DEPENDS run_rimage)
endif()

add_custom_target(
	bin
	COMMAND ${CMAKE_COMMAND} -E copy sof-${fw_name}.ri ${PROJECT_BINARY_DIR}/sof-${fw_name}.ri
	DEPENDS run_meu
	VERBATIM
	USES_TERMINAL
)

if(CONFIG_BUILD_VM_ROM)
	add_executable(rom "")
	target_link_libraries(rom PRIVATE sof_options)
	target_link_libraries(rom PRIVATE "-lgcc")
	target_link_libraries(rom PRIVATE "-T${PROJECT_BINARY_DIR}/${platform_rom_ld_script}")
	sof_add_ld_script(rom ${platform_rom_ld_script})

	# We have to make additional define, because sources
	# are reused for other objects with different flags.
	target_compile_definitions(rom PRIVATE -DCONFIG_VM_ROM)

	if(CONFIG_SMP)
		# SMP ROM uses UP CRT1
		add_local_sources(rom
			smp/xtos/memctl_default.S
			smp/xtos/reset-vector.S
			up/xtos/crt1-boards.S
		)
	else()
		add_local_sources(rom
			up/xtos/reset-vector.S
			up/xtos/crt1-boards.S
		)
	endif()

	add_custom_target(
		rom_dump
		COMMAND ${CMAKE_COMMAND} -E copy rom rom-${fw_name}
		COMMAND ${CMAKE_OBJCOPY} -O binary rom rom-${fw_name}.bin
		COMMAND ${CMAKE_OBJDUMP} -h -D rom > rom-${fw_name}.map
		COMMAND ${CMAKE_OBJDUMP} -S rom > rom-${fw_name}.lst
		COMMAND ${CMAKE_OBJDUMP} -D rom > rom-${fw_name}.dis
		DEPENDS rom
		VERBATIM
		USES_TERMINAL
	)

	add_dependencies(bin rom_dump)
endif()
