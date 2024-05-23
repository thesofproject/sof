# SPDX-License-Identifier: BSD-3-Clause

set(TPLGS
	## i.MX8 (i.MX8QM and i.MX8QXP) topologies
	"sof-imx8-src-wm8960\;sof-imx8-src-wm8960\;-DCODEC=wm8960\;-DRATE=48000\;-DPPROC=src\;-DSAI_INDEX=1"
	"sof-imx8-src-wm8960\;sof-imx8-src-wm8962\;-DCODEC=wm8962\;-DRATE=48000\;-DPPROC=src\;-DSAI_INDEX=1"
	"sof-imx8-src-cs42888\;sof-imx8-src-cs42888"
	"sof-imx8-compr-pcm-wm8960\;sof-imx8-compr-pcm-wm8960"
	"sof-imx8-compr-pcm-cap-wm8960\;sof-imx8-compr-pcm-cap-wm8960"
	"sof-imx8-compr-wm8960\;sof-imx8-compr-wm8960\;-DCODEC=wm8960\;-DRATE=48000"
	"sof-imx8-compr-wm8960\;sof-imx8-compr-wm8962\;-DCODEC=wm8962\;-DRATE=48000"
	## end i.MX8 (i.MX8QM and i.MX8QXP) topologies

	## i.MX8MP topologies
	"sof-imx8-src-wm8960\;sof-imx8mp-src-wm8960\;-DCODEC=wm8960\;-DRATE=48000\;-DPPROC=src\;-DSAI_INDEX=3\;-DDMA_DOMAIN"
	"sof-imx8-src-wm8960\;sof-imx8mp-src-wm8962\;-DCODEC=wm8962\;-DRATE=48000\;-DPPROC=src\;-DSAI_INDEX=3\;-DDMA_DOMAIN"
	"sof-imx8mp-compr-pcm-wm8960\;sof-imx8mp-compr-pcm-wm8960"
	"sof-imx8mp-compr-pcm-cap-wm8960\;sof-imx8mp-compr-pcm-cap-wm8960"
	"sof-imx8mp-compr-wm8960\;sof-imx8mp-compr-wm8960\;-DCODEC=wm8960\;-DRATE=48000"
	"sof-imx8mp-compr-wm8960\;sof-imx8mp-compr-wm8962\;-DCODEC=wm8962\;-DRATE=48000"
	## end i.MX8MP topologies
)


add_custom_target(dev_topologies1 ALL)

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
		COMMAND m4 --fatal-warnings
			${DEFINES}
			-I ${CMAKE_CURRENT_SOURCE_DIR}/../m4
			-I ${CMAKE_CURRENT_SOURCE_DIR}/../common
			-I ${CMAKE_CURRENT_SOURCE_DIR}/../platform/common
			-I ${CMAKE_CURRENT_SOURCE_DIR}/../
			-I ${CMAKE_CURRENT_BINARY_DIR}/../
			${CMAKE_CURRENT_SOURCE_DIR}/../common/abi.m4
			${CMAKE_CURRENT_SOURCE_DIR}/${input}.m4
			> ${output}.conf
		DEPENDS abi_v1 ${CMAKE_BINARY_DIR}/topology/topology1/abi.h
		VERBATIM
		USES_TERMINAL
	)

	add_alsatplg_command(${output}.conf ${output}.tplg)

	add_custom_target(dev_topology_${output} DEPENDS ${output}.tplg)
	add_dependencies(dev_topologies1 dev_topology_${output})
endforeach()
