
add_library(template MODULE)

add_local_sources(template module.c template.c)

# define compiler version
set_property(TARGET template APPEND
	PROPERTY COMPILE_DEFINITIONS
	XCC_TOOLS_VERSION="${XCC_TOOLS_VERSION}")

get_optimization_flag(optimization_flag)
set_property(TARGET template APPEND
	PROPERTY COMPILE_DEFINITIONS
	CC_OPTIMIZE_FLAGS="-${optimization_flag}")

set(module_plat_name ${CONFIG_RIMAGE_SIGNING_SCHEMA})
set(module_name "template")

if(NOT DEFINED RIMAGE_PRIVATE_KEY)
	set(RIMAGE_PRIVATE_KEY ${PROJECT_SOURCE_DIR}/keys/otc_private_key.pem)
endif()

if(NOT DEFINED RIMAGE_IMR_TYPE)
	# default value for non-production firmware
	set(RIMAGE_IMR_TYPE 3)
endif()

target_include_directories(template PRIVATE ${PROJECT_SOURCE_DIR}/rimage/src/include)
target_include_directories(template PRIVATE ${PROJECT_SOURCE_DIR}/src/include)
target_include_directories(template PRIVATE ${PROJECT_SOURCE_DIR}/src/arch/${ARCH}/include)
target_include_directories(template PRIVATE ${PROJECT_SOURCE_DIR}/src/platform/${platform_folder}/include)
target_include_directories(template PRIVATE ${PROJECT_SOURCE_DIR}/src/platform/${platform_folder}/include/arch)
target_include_directories(template PRIVATE ${PROJECT_SOURCE_DIR}/src/platform/${family_path}/include)

target_include_directories(template PRIVATE
	${PROJECT_SOURCE_DIR}/src/arch/xtensa/include
	${PROJECT_SOURCE_DIR}/src/arch/xtensa/xtos
)
target_include_directories(template PRIVATE ${PROJECT_BINARY_DIR}/generated/include)

# No space between -imacros and its argument to avoid CMake
# de-duplication "feature"
target_compile_options(template PRIVATE
	$<$<COMPILE_LANGUAGE:C>:
		-${optimization_flag} -g
		-Wall -Werror
		-Wl,-EL
		-Wmissing-prototypes
		-Wpointer-arith
		${XTENSA_C_FLAGS}
		${EXTRA_CFLAGS_AS_LIST}
	>
	-imacros${CONFIG_H_PATH}
	)

sof_add_module_ld_script(template template.x)
sof_append_relative_path_definitions(template)

# create the relocatable object and strip debug symbols 
add_custom_target(
	make_module_object
	COMMAND ${CMAKE_LD} --strip-debug -relocatable --strip-discarded
		-T${PROJECT_BINARY_DIR}/template.x
		${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/template.dir/template.c.o
		${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/template.dir/module.c.o
		-o ${CMAKE_CURRENT_BINARY_DIR}/${module_name}.mod
	DEPENDS template
	VERBATIM
	USES_TERMINAL
)

add_custom_target(
	run_module_rimage ALL
	COMMAND ${PROJECT_BINARY_DIR}/rimage_ep/build/rimage
		-o module-${module_name}-${module_plat_name}.ri
		-c "${PROJECT_SOURCE_DIR}/rimage/config/module-template.toml"
		-k ${RIMAGE_PRIVATE_KEY}
		-i ${RIMAGE_IMR_TYPE}
		-f ${SOF_MAJOR}.${SOF_MINOR}
		-b ${SOF_BUILD}
		-e
		-v
		-r
		${CMAKE_CURRENT_BINARY_DIR}/${module_name}.mod
	DEPENDS sof_post_process rimage_ep make_module_object
	VERBATIM
	USES_TERMINAL
)
