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

# Builds lib for each level from the same source files
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
)
target_link_libraries(xtos sof_options)
target_compile_definitions(xtos PRIVATE ${VECTOR_DEFS})
