set(VECTOR_DEFS
	-D__SPLIT__vector
	-D__SPLIT__handler
	-D__SPLIT__user
	-D__SPLIT__level1int
	-D__SPLIT__level2
	-D__SPLIT__level3
	-D__SPLIT__level4
	-D__SPLIT__level5
)

if(CONFIG_APOLLOLAKE
   OR CONFIG_CANNONLAKE
   OR CONFIG_SUECREEK
   OR CONFIG_ICELAKE)
	set(VECTOR_DEFS ${VECTOR_DEFS} -D__SPLIT__level6)
endif()

# Builds lib for each level from single the same source files
# TODO: should there be level 6 for apl+ ?

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

set(levels 2 3 4 5)

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
	exc-alloca-handler.S
	exc-c-wrapper-handler.S
	exc-return.S
	exc-sethandler.c
	exc-syscall-c-handler.c
	exc-syscall-handler.S
	exc-table.S
	exc-unhandled.S
	init.c
	interrupt-table.S
	int-sethandler.c
	intlevel-restore.S
	intlevel-setmin.S
	intlevel-set.S
	int-lowpri-dispatcher.S
	ints-off.S
	ints-on.S
	kernel-vector.S
	memep-enable.S
	memep-initrams.S
	memerror-vector.S
	nmi-vector.S
	reloc-vectors.S
	user-vector.S
	window-vectors.S
)
target_link_libraries(xtos sof_options)
target_compile_definitions(xtos PRIVATE ${VECTOR_DEFS})
