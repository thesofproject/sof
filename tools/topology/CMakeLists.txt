# SPDX-License-Identifier: BSD-3-Clause

file(GLOB TPLG_DEPS
	platform/intel/*.m4
	platform/common/*.m4
	common/*.m4
	m4/*.m4
	sof/*.m4
)

add_custom_target(abi
	COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/get_abi.sh ${SOF_ROOT_SOURCE_DIRECTORY}
	DEPENDS ${TPLG_DEPS}
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	VERBATIM
	USES_TERMINAL
)

set(TPLGS
	"sof-cht-nocodec\;sof-cht-nocodec\;-DPLATFORM=cht"
	"sof-cht-nocodec\;sof-byt-nocodec\;-DPLATFORM=byt"
	"sof-cht-max98090\;sof-cht-max98090"
	"sof-cht-rt5682\;sof-cht-rt5682"
	"sof-hda-generic\;sof-hda-generic\;-DCHANNELS=0"
	"sof-hda-generic\;sof-hda-generic-2ch\;-DCHANNELS=2"
	"sof-hda-generic\;sof-hda-generic-4ch\;-DCHANNELS=4"
	"sof-hda-generic-idisp\;sof-hda-generic-idisp\;-DCHANNELS=0"
	"sof-hda-generic-idisp\;sof-hda-generic-idisp-2ch\;-DCHANNELS=2"
	"sof-hda-generic-idisp\;sof-hda-generic-idisp-4ch\;-DCHANNELS=4"
	"sof-apl-nocodec\;sof-apl-nocodec"
	"sof-apl-keyword-detect\;sof-apl-keyword-detect"
	"sof-bdw-codec\;sof-bdw-rt286\;-DCODEC=RT286"
	"sof-bdw-codec\;sof-bdw-rt5640\;-DCODEC=RT5640"
	"sof-byt-codec\;sof-byt-rt5640\;-DCODEC=RT5640"
	"sof-byt-codec\;sof-byt-rt5645\;-DCODEC=RT5645"
	"sof-byt-codec\;sof-byt-rt5651\;-DCODEC=RT5651"
	"sof-byt-codec\;sof-byt-rt5682\;-DCODEC=RT5682"
	"sof-byt-codec\;sof-byt-da7213\;-DCODEC=DA7213"
	"sof-cnl-rt274\;sof-cnl-rt274"
	"sof-hsw-rt5640\;sof-hsw-rt5640"
	"sof-apl-tdf8532\;sof-apl-tdf8532"
	"sof-apl-pcm512x\;sof-apl-pcm512x"
	"sof-apl-pcm512x-nohdmi\;sof-apl-pcm512x-nohdmi"
	"sof-apl-demux-pcm512x\;sof-apl-demux-pcm512x"
	"sof-apl-rt298\;sof-apl-rt298"
	"sof-apl-wm8804\;sof-apl-wm8804"
	"sof-apl-da7219\;sof-apl-da7219"
	"sof-glk-da7219-kwd\;sof-glk-da7219-kwd"
	"sof-glk-da7219\;sof-glk-da7219"
	"sof-glk-rt5682\;sof-glk-rt5682"
	"sof-icl-nocodec\;sof-icl-nocodec"
	"sof-apl-eq-pcm512x\;sof-apl-eq-pcm512x"
	"sof-apl-eq-dmic\;sof-apl-eq-dmic"
	"sof-apl-dmic-a2ch\;sof-apl-dmic-a2ch"
	"sof-apl-dmic-b2ch\;sof-apl-dmic-b2ch"
	"sof-apl-dmic-a2ch-b2ch\;sof-apl-dmic-a2ch-b2ch"
	"sof-apl-dmic-a4ch-b2ch\;sof-apl-dmic-a4ch-b2ch"
	"sof-apl-dmic-a2ch-b4ch\;sof-apl-dmic-a2ch-b4ch"
	"sof-apl-dmic\;sof-apl-dmic-4ch\;-DCHANNELS=4"
	"sof-apl-dmic\;sof-apl-dmic-2ch\;-DCHANNELS=2"
	"sof-apl-src-50khz-pcm512x\;sof-apl-src-50khz-pcm512x"
	"sof-icl-dmic-4ch\;sof-icl-dmic-4ch"
	"sof-icl-rt700\;sof-icl-rt700\;-DPLATFORM=icl"
	"sof-cml-rt700\;sof-cml-rt700\;-DPLATFORM=cml"
	"sof-icl-rt711-rt1308-rt715\;sof-icl-rt711-rt1308-rt715\;-DPLATFORM=icl"
	"sof-apl-src-pcm512x\;sof-apl-src-pcm512x"
	"sof-cml-rt5682\;sof-cml-rt5682\;-DPLATFORM=cml"
	"sof-cml-rt5682\;sof-whl-rt5682\;-DPLATFORM=whl"
	"sof-cml-rt5682\;sof-icl-rt5682\;-DPLATFORM=icl"
	"sof-cml-rt5682-kwd\;sof-cml-rt5682-kwd\;-DPLATFORM=cml"
	"sof-cml-rt5682-kwd\;sof-whl-rt5682-kwd\;-DPLATFORM=whl"
	"sof-cml-rt5682-kwd\;sof-icl-rt5682-kwd\;-DPLATFORM=icl"
	"sof-cml-src-rt5682\;sof-cml-src-rt5682"
	"sof-cml-demux-rt5682\;sof-cml-demux-rt5682\;-DPLATFORM=whl"
	"sof-cnl-nocodec\;sof-cnl-nocodec"
	"sof-cml-rt5682-max98357a\;sof-cml-rt5682-max98357a\;-DPLATFORM=cml"
	"sof-cml-demux-rt5682-max98357a\;sof-cml-demux-rt5682-max98357a\;-DPLATFORM=cml"
	"sof-tgl-nocodec\;sof-tgl-nocodec"
	"sof-tgl-rt1308\;sof-tgl-rt1308"
	"sof-tgl-rt1308-nohdmi\;sof-tgl-rt1308-nohdmi"
	"sof-imx8-nocodec\;sof-imx8-nocodec\;-DPLATFORM=imx8"
)

add_custom_target(topologies ALL)

foreach(tplg ${TPLGS})
	list(GET tplg 0 input)
	list(GET tplg 1 output)
	list(LENGTH tplg tplg_len)
	math(EXPR num_args "${tplg_len} - 1")
	set(DEFINES "")
	if (${num_args} GREATER 1)
		foreach(index RANGE 2 ${num_args})
			list(GET tplg ${index} custom_define)
			list(APPEND DEFINES ${custom_define})
		endforeach()
	endif()

	add_custom_command(
		OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${output}.conf
		COMMAND m4
			${DEFINES}
			-I ${CMAKE_CURRENT_SOURCE_DIR}/m4
			-I ${CMAKE_CURRENT_SOURCE_DIR}/common
			-I ${CMAKE_CURRENT_SOURCE_DIR}/platform/common
			-I ${CMAKE_CURRENT_SOURCE_DIR}
			-I ${CMAKE_CURRENT_BINARY_DIR}
			${CMAKE_CURRENT_SOURCE_DIR}/common/abi.m4
			${CMAKE_CURRENT_SOURCE_DIR}/${input}.m4
			> ${output}.conf
		DEPENDS abi
		VERBATIM
		USES_TERMINAL
	)

	add_custom_command(
		OUTPUT ${output}.tplg
		COMMAND alsatplg -v 1 -c ${output}.conf -o ${output}.tplg
		DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${output}.conf
		VERBATIM
		USES_TERMINAL
	)

	add_custom_target(topology_${output} DEPENDS ${output}.tplg)
	add_dependencies(topologies topology_${output})
endforeach()
