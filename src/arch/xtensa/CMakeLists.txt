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
		COMMAND cat ${lds_in} | ${CMAKE_C_COMPILER} -E -P ${iflags} - > ${lds_out}
		DEPENDS ${lds_in} ${LINK_DEPS} genconfig
		WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
		COMMENT "Generating linker script: ${lds_out}"
		VERBATIM
		USES_TERMINAL
	)

	add_custom_target("ld_script_${script_name}" DEPENDS ${lds_out})
	add_dependencies(${binary_name} "ld_script_${script_name}")
endfunction()

sof_add_ld_script(sof ${platform_ld_script})

if(CONFIG_BUILD_VM_ROM)
	sof_add_ld_script(${platform_rom_ld_script})
endif()

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

if(CONFIG_SMP)
# TODO it should be handled in subdir
	add_subdirectory(smp)
	add_local_sources(sof
		smp/xtos/crt1-boards.S
		smp/xtos/_vectors.S
		smp/cpu.c
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
