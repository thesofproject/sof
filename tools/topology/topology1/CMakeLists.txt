# SPDX-License-Identifier: BSD-3-Clause

file(GLOB TPLG_DEPS
	platform/common/*.m4
	common/*.m4
	m4/*.m4
	sof/*.m4
)

add_custom_target(abi_v1
	DEPENDS abi.h
)
add_custom_command(OUTPUT abi.h
	COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/get_abi.sh ${SOF_ROOT_SOURCE_DIRECTORY}
	DEPENDS ${TPLG_DEPS} ${SOF_ROOT_SOURCE_DIRECTORY}/src/include/kernel/abi.h
	# get_abi.h generates (a smaller) abi.h in the current directory
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	VERBATIM
	USES_TERMINAL
)

set(TPLGS
	## i.MX8 (i.MX8QM and i.MX8QXP) topologies
	"sof-imx8-nocodec\;sof-imx8-nocodec"
	"sof-imx8-wm8960-cs42888\;sof-imx8-wm8960-cs42888"
	"sof-imx8-cs42888\;sof-imx8-cs42888"
	"sof-imx8-nocodec-sai\;sof-imx8-nocodec-sai"
	"sof-imx8-wm8960-mixer\;sof-imx8-wm8960-mixer\;-DCODEC=wm8960\;-DRATE=48000\;-DSAI_INDEX=1"
	"sof-imx8-wm8960-mixer\;sof-imx8-wm8962-mixer\;-DCODEC=wm8962\;-DRATE=48000\;-DSAI_INDEX=1"
	"sof-imx8-compr-wm8960-mixer\;sof-imx8-compr-wm8960-mixer\;-DCODEC=wm8960\;-DSAI_INDEX=1"
	"sof-imx8-compr-wm8960-mixer\;sof-imx8-compr-wm8962-mixer\;-DCODEC=wm8962\;-DSAI_INDEX=1"
	"sof-imx8-wm8960-kwd\;sof-imx8-wm8960-kwd"
	"sof-imx8-cs42888-mixer\;sof-imx8-cs42888-mixer"
	"sof-imx8-wm8960\;sof-imx8-wm8960\;-DCODEC=wm8960\;-DRATE=48000\;-DPPROC=volume\;-DSAI_INDEX=1"
	"sof-imx8-wm8960\;sof-imx8-wm8962\;-DCODEC=wm8962\;-DRATE=48000\;-DPPROC=volume\;-DSAI_INDEX=1"
	"sof-imx8-wm8960\;sof-imx8-eq-iir-wm8960\;-DCODEC=wm8960\;-DRATE=48000\;-DPPROC=eq-iir-volume\;-DSAI_INDEX=1"
	"sof-imx8-wm8960\;sof-imx8-eq-iir-wm8962\;-DCODEC=wm8962\;-DRATE=48000\;-DPPROC=eq-iir-volume\;-DSAI_INDEX=1"
	"sof-imx8-wm8960\;sof-imx8-eq-fir-wm8960\;-DCODEC=wm8960\;-DRATE=48000\;-DPPROC=eq-fir-volume\;-DSAI_INDEX=1"
	"sof-imx8-wm8960\;sof-imx8-eq-fir-wm8962\;-DCODEC=wm8962\;-DRATE=48000\;-DPPROC=eq-fir-volume\;-DSAI_INDEX=1"
	"sof-imx8-wm8960\;sof-imx8-drc-wm8960\;-DCODEC=wm8960\;-DRATE=48000\;-DPPROC=drc\;-DSAI_INDEX=1"
	## end i.MX8 (i.MX8QM and i.MX8QXP) topologies

	## i.MX8MP topologies
	"sof-imx8-compr-wm8960-mixer\;sof-imx8mp-compr-wm8960-mixer\;-DCODEC=wm8960\;-DSAI_INDEX=3"
	"sof-imx8-compr-wm8960-mixer\;sof-imx8mp-compr-wm8962-mixer\;-DCODEC=wm8962\;-DSAI_INDEX=3"
	"sof-imx8-wm8960-mixer\;sof-imx8mp-wm8960-mixer\;-DCODEC=wm8960\;-DRATE=48000\;-DSAI_INDEX=3"
	"sof-imx8-wm8960-mixer\;sof-imx8mp-wm8962-mixer\;-DCODEC=wm8962\;-DRATE=48000\;-DSAI_INDEX=3"
	"sof-imx8mp-wm8960-kwd\;sof-imx8mp-wm8960-kwd"
	"sof-imx8mp-micfil\;sof-imx8mp-micfil"
	"sof-imx8mp-btsco-dual-8ch\;sof-imx8mp-btsco-dual-8ch"
	"sof-imx8-wm8960\;sof-imx8mp-wm8960\;-DCODEC=wm8960\;-DRATE=48000\;-DPPROC=volume\;-DSAI_INDEX=3"
	"sof-imx8-wm8960\;sof-imx8mp-wm8904\;-DCODEC=wm8904\;-DRATE=44100\;-DPPROC=volume\;-DSAI_INDEX=3"
	"sof-imx8-wm8960\;sof-imx8mp-wm8962\;-DCODEC=wm8962\;-DRATE=48000\;-DPPROC=volume\;-DSAI_INDEX=3"
	"sof-imx8-wm8960\;sof-imx8mp-eq-iir-wm8960\;-DCODEC=wm8960\;-DRATE=48000\;-DPPROC=eq-iir-volume\;-DSAI_INDEX=3"
	"sof-imx8-wm8960\;sof-imx8mp-eq-iir-wm8962\;-DCODEC=wm8962\;-DRATE=48000\;-DPPROC=eq-iir-volume\;-DSAI_INDEX=3"
	"sof-imx8-wm8960\;sof-imx8mp-eq-fir-wm8960\;-DCODEC=wm8960\;-DRATE=48000\;-DPPROC=eq-fir-volume\;-DSAI_INDEX=3"
	"sof-imx8-wm8960\;sof-imx8mp-eq-fir-wm8962\;-DCODEC=wm8962\;-DRATE=48000\;-DPPROC=eq-fir-volume\;-DSAI_INDEX=3"
	"sof-imx8-wm8960\;sof-imx8mp-drc-wm8960\;-DCODEC=wm8960\;-DRATE=48000\;-DPPROC=drc\;-DSAI_INDEX=3"
	## end i.MX8MP topologies

	## i.MX8ULP topologies
	"sof-imx8ulp-btsco\;sof-imx8ulp-btsco\;-DRATE=8000"
	"sof-imx8ulp-btsco\;sof-imx8ulp-btsco-16k\;-DRATE=16000"
	"sof-imx8ulp-9x9-btsco\;sof-imx8ulp-9x9-btsco\;-DRATE=8000"
	"sof-imx8ulp-9x9-btsco\;sof-imx8ulp-9x9-btsco-16k\;-DRATE=16000"
	## end i.MX8ULP topologies

	## i.MX93 topologies
	"sof-imx93-wm8962\;sof-imx93-wm8962\;-DPPROC=volume"
	## end i.MX93 topologies

	"sof-mt8195-mt6359-rt1019-rt5682\;sof-mt8195-mt6359-rt1019-rt5682"
	"sof-mt8195-mt6359-rt1019-rt5682\;sof-mt8195-mt6359-rt1019-rt5682-dts\;-DDTS=`DTS'"
        "sof-mt8195-mt6359-rt1019-rt5682\;sof-mt8195-mt6359-max98390-rt5682"
	"sof-mt8195-mt6359-rt1019-rt5682\;sof-mt8195-mt6359-max98390-rt5682-rtnr\;-DCHANNELS=2\;-DRTNR"
	"sof-mt8195-mt6359-rt1019-rt5682\;sof-mt8195-mt6359-max98390-rt5682-google-aec-rtnr\;-DGOOGLE_RTC_AUDIO_PROCESSING\;-DCHANNELS=2\;-DRTNR"
	"sof-mt8186-mt6366\;sof-mt8186"
	"sof-mt8186-mt6366\;sof-mt8186-dts\;-DDTS=`DTS'"
	"sof-mt8186-mt6366\;sof-mt8186-mt6366-rt1019-rt5682s-waves\;-DWAVES=1"
	"sof-mt8188-mt6359\;sof-mt8188"
	"sof-mt8188-mt6359\;sof-mt8188-waves\;-DWAVES=1"

	"sof-acp-renoir\;sof-acp"
	"sof-rn-rt5682-rt1019\;sof-rn-rt5682-rt1019"
	"sof-rn-rt5682-rt1019\;sof-rn-rt5682-rt1019-dts\;-DDTS=`DTS'"
	"sof-rn-rt5682-max98360\;sof-rn-rt5682-max98360"
	"sof-acp-vangogh\;sof-vangogh-nau8821-max"
	"sof-acp-rmb\;sof-acp-rmb"
	"sof-acp-rmb-dmic4ch\;sof-acp-rmb-dmic4ch"
	"sof-acp-rmb-tdm8ch-dmic4ch-rt5682-rt1019\;sof-acp-rmb-tdm8ch-dmic4ch-rt5682-rt1019"
	"sof-acp-rmb-tdm8ch-dmic2ch-nau8825-max98360\;sof-acp-rmb-tdm8ch-dmic2ch-nau8825-max98360"
	"sof-acp-rmb\;sof-rmb-rt5682s-rt1019"
	"sof-acp-rmb\;sof-rmb-nau8825-max98360"
	"sof-acp_6_3\;sof-acp_6_3"
	"sof-acp_6_3_sdw\;sof-acp_6_3-rt715-rt711-rt1316"
)

# This empty 'production/' source subdirectory exists only to create the
# corresponding 'production/' subdirectory in the build directory and
# solve race https://github.com/thesofproject/sof/issues/5067 without
# the disruption of actually moving all *.m4 sources one directory level
# down.
add_subdirectory(production)

# "Install" all v1 production topologies two directory levels up.
add_custom_target(topologies1 ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory production ${SOF_TOPOLOGY_BINARY_DIRECTORY}
)

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

	# m4
	# TODO: reduce duplication with subdirectories
	add_custom_command(
		OUTPUT production/${output}.conf
		COMMAND m4 --fatal-warnings
			${DEFINES}
			-I ${CMAKE_CURRENT_SOURCE_DIR}/m4
			-I ${CMAKE_CURRENT_SOURCE_DIR}/common
			-I ${CMAKE_CURRENT_SOURCE_DIR}/platform/common
			-I ${CMAKE_CURRENT_SOURCE_DIR}
			-I ${CMAKE_CURRENT_BINARY_DIR}
			${CMAKE_CURRENT_SOURCE_DIR}/common/abi.m4
			${CMAKE_CURRENT_SOURCE_DIR}/${input}.m4
			> production/${output}.conf
		MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/${input}.m4
		# The 'abi.h' file is the real dependency that triggers
		# a rebuild. 'abi_v1' is the target that avoids a race
		# to rebuild abi.h. See the CMake FAQ or (better) Sam
		# Thursfield's blog about custom targets.
		DEPENDS abi_v1 ${CMAKE_CURRENT_BINARY_DIR}/abi.h
		VERBATIM
		USES_TERMINAL
	)

	# alsatplg
	add_alsatplg_command(production/${output}.conf production/${output}.tplg)

	add_custom_target(topology1_${output} DEPENDS production/${output}.tplg)
	add_dependencies(topologies1 topology1_${output})
endforeach()

add_subdirectory(development)
