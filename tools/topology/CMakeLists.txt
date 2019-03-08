file(GLOB TPLG_DEPS
	platform/intel/*.m4
	platform/common/*.m4
	common/*.m4
	m4/*.m4
	sof/*.m4
)

set(MACHINES
	sof-cht-nocodec
	sof-cht-max98090
	sof-cht-rt5682
	sof-hda-intel-common
	sof-apl-nocodec
	sof-byt-nocodec
	sof-bdw-rt286
	sof-bdw-rt5640
	sof-byt-rt5640
	sof-byt-rt5645
	sof-byt-rt5651
	sof-byt-da7213
	sof-cnl-rt274
	sof-hsw-rt5640
	sof-apl-tdf8532
	sof-apl-pcm512x
	sof-apl-pcm512x-nohdmi
	sof-apl-rt298
	sof-apl-wm8804
	sof-apl-da7219
	sof-glk-da7219
	sof-glk-rt5682
	sof-icl-nocodec
	sof-apl-eq-pcm512x
	sof-apl-eq-dmic
	sof-apl-dmic-2ch
	sof-apl-dmic-4ch
	sof-icl-dmic-4ch
	sof-apl-src-pcm512x
	sof-cml-rt5682
	sof-whl-rt5682
	sof-cml-src-rt5682
	sof-cnl-nocodec
	sof-glk-src-max98357
)

add_custom_target(topologies ALL)

foreach(machine ${MACHINES})
	add_custom_command(
		OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${machine}.conf
		COMMAND m4
			-I ${CMAKE_CURRENT_SOURCE_DIR}/m4
			-I ${CMAKE_CURRENT_SOURCE_DIR}/common
			-I ${CMAKE_CURRENT_SOURCE_DIR}/platform/common
			-I ${CMAKE_CURRENT_SOURCE_DIR}
			${CMAKE_CURRENT_SOURCE_DIR}/${machine}.m4
			> ${machine}.conf
		DEPENDS ${TPLG_DEPS}
		VERBATIM
		USES_TERMINAL
	)

	add_custom_command(
		OUTPUT ${machine}.tplg
		COMMAND alsatplg -v 1 -c ${machine}.conf -o ${machine}.tplg
		DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${machine}.conf
		VERBATIM
		USES_TERMINAL
	)

	add_custom_target(topology_${machine} DEPENDS ${machine}.tplg)
	add_dependencies(topologies topology_${machine})
endforeach()
