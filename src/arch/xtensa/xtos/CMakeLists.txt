# SPDX-License-Identifier: BSD-3-Clause

set(VECTOR_DEFS
	-D__SPLIT__vector
	-D__SPLIT__handler
	-D__SPLIT__user
)

# Builds lib for each level from the same source files

function(sof_xtos_add_level level)
	set(lib_name "xlevel${level}")
	add_library(${lib_name} STATIC "")
	add_local_sources(${lib_name}
		int-handler.S
		int-vector.S
		int-initlevel.S
	)
	target_link_libraries(${lib_name} sof_options)
	target_compile_definitions(${lib_name} PRIVATE
		${VECTOR_DEFS}
		-D_INTERRUPT_LEVEL=${level}
	)
endfunction()

if(CONFIG_XT_INTERRUPT_LEVEL_1)
	list(APPEND VECTOR_DEFS -D__SPLIT__level1int)
endif()

if(CONFIG_XT_INTERRUPT_LEVEL_2)
	list(APPEND VECTOR_DEFS -D__SPLIT__level2)
	list(APPEND levels 2)
endif()

if(CONFIG_XT_INTERRUPT_LEVEL_3)
	list(APPEND VECTOR_DEFS -D__SPLIT__level3)
	list(APPEND levels 3)
endif()

if(CONFIG_XT_INTERRUPT_LEVEL_4)
	list(APPEND VECTOR_DEFS -D__SPLIT__level4)
	list(APPEND levels 4)
endif()

if(CONFIG_XT_INTERRUPT_LEVEL_5)
	list(APPEND VECTOR_DEFS -D__SPLIT__level5)
	list(APPEND levels 5)
endif()

foreach(level ${levels})
	sof_xtos_add_level(${level})
endforeach()

add_library(xtos STATIC "")
add_local_sources(xtos
	core-restore.S
	core-save.S
	core-shutoff.S
	double-vector.S
	debug-vector.S
	xea1/exc-alloca-handler.S
	xea1/exc-c-wrapper-handler.S
	xea2/exc-c-wrapper-handler.S
	xea1/exc-return.S
	xea2/exc-return.S
	exc-sethandler.c
	exc-syscall-handler.S
	exc-table.S
	exc-unhandled.S
	interrupt-table.S
	int-sethandler.c
	xea1/intlevel-restore.S
	xea2/intlevel-restore.S
	xea1/intlevel-setmin.S
	xea2/intlevel-setmin.S
	xea1/intlevel-set.S
	xea2/intlevel-set.S
	xea1/int-lowpri-dispatcher.S
	xea2/int-lowpri-dispatcher.S
	ints-off.S
	ints-on.S
	kernel-vector.S
	memep-enable.S
	memep-initrams.S
	memerror-vector.S
	nmi-vector.S
	xea2/reloc-vectors.S
	user-vector.S
	xea1/window-vectors.S
	xea2/window-vectors.S
	stub.c
)
sof_append_relative_path_definitions(xtos)
target_link_libraries(xtos sof_options)
target_compile_definitions(xtos PRIVATE ${VECTOR_DEFS})

