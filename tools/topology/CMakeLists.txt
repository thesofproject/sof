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
	"sof-cht-nocodec\;sof-cht-nocodec\;-DPLATFORM=cht-nocodec\;-DSSP_NUM=2"
	"sof-cht-nocodec\;sof-byt-nocodec\;-DPLATFORM=byt-nocodec\;-DSSP_NUM=2"
	"sof-bdw-nocodec\;sof-bdw-nocodec"
	"sof-hda-generic\;sof-hda-generic\;-DCHANNELS=0\;-DPPROC=volume"
	"sof-hda-generic\;sof-hda-generic-1ch\;-DCHANNELS=2\;-DPPROC=volume\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4"
	"sof-hda-generic\;sof-hda-generic-2ch\;-DCHANNELS=2\;-DPPROC=volume\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4"
	"sof-hda-generic\;sof-hda-generic-3ch\;-DCHANNELS=4\;-DPPROC=volume\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4"
	"sof-hda-generic\;sof-hda-generic-4ch\;-DCHANNELS=4\;-DPPROC=volume\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4"
	"sof-hda-generic\;sof-hda-generic-eq-2ch\;-DCHANNELS=2\;-DPPROC=eq-iir-eq-fir-volume\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4"
	"sof-hda-generic\;sof-hda-generic-eq-4ch\;-DCHANNELS=4\;-DPPROC=eq-iir-eq-fir-volume\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4"
	"sof-hda-generic\;sof-hda-generic-tdfb_50mm-2ch\;-DCHANNELS=2\;-DPPROC=volume\;-DDMIC16KPROC=tdfb-eq-iir-volume\;-DDMIC16KPROC_FILTER1=tdfb/coef_line2_50mm_pm10deg_16khz.m4\;-DDMICPROC=tdfb-eq-iir-volume\;-DDMICPROC_FILTER1=tdfb/coef_line2_50mm_pm10deg_48khz.m4\;-DDMICPROC_FILTER2=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER2=eq_iir_coef_highpass_40hz_20db_16khz.m4"
	"sof-hda-generic-kwd\;sof-hda-generic-2ch-kwd\;-DCHANNELS=2"
	"sof-hda-generic-kwd\;sof-hda-generic-4ch-kwd\;-DCHANNELS=4"
	"sof-hda-generic-idisp\;sof-hda-generic-idisp\;-DCHANNELS=0"
	"sof-hda-generic-idisp\;sof-hda-generic-idisp-2ch\;-DCHANNELS=2\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4"
	"sof-hda-generic-idisp\;sof-hda-generic-idisp-4ch\;-DCHANNELS=4\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4"
	"sof-hda-asrc\;sof-hda-asrc-2ch\;-DCHANNELS=2"
	"sof-apl-nocodec\;sof-apl-nocodec"
	"sof-apl-keyword-detect\;sof-apl-keyword-detect"
	"sof-bdw-codec\;sof-bdw-rt286\;-DCODEC=RT286"
	"sof-bdw-codec\;sof-bdw-rt5640\;-DCODEC=RT5640"
	"sof-bdw-codec\;sof-bdw-rt5677\;-DCODEC=RT5677"
	"sof-byt-codec\;sof-byt-rt5640\;-DCODEC=RT5640\;-DPLATFORM=byt-codec\;-DSSP_NUM=2"
	"sof-byt-codec\;sof-byt-rt5645\;-DCODEC=RT5645\;-DPLATFORM=byt-codec\;-DSSP_NUM=2"
	"sof-byt-codec\;sof-byt-rt5651\;-DCODEC=RT5651\;-DPLATFORM=byt-codec\;-DSSP_NUM=2"
	"sof-byt-codec\;sof-byt-rt5670\;-DCODEC=RT5670\;-DPLATFORM=byt-codec\;-DSSP_NUM=2"
	"sof-byt-codec\;sof-byt-rt5682\;-DCODEC=RT5682\;-DPLATFORM=byt-codec\;-DSSP_NUM=2"
	"sof-byt-codec\;sof-byt-da7213\;-DCODEC=DA7213\;-DPLATFORM=byt-codec\;-DSSP_NUM=2"
	"sof-byt-codec\;sof-byt-cx2072x\;-DCODEC=CX2072X\;-DPLATFORM=byt-codec\;-DSSP_NUM=2"
	"sof-byt-codec\;sof-byt-es8316\;-DCODEC=ES8316\;-DPLATFORM=byt-codec\;-DSSP_NUM=2"
	"sof-byt-codec\;sof-byt-rt5640-ssp0\;-DCODEC=RT5640\;-DPLATFORM=byt-codec\;-DSSP_NUM=0"
	"sof-byt-codec\;sof-byt-rt5645-ssp0\;-DCODEC=RT5645\;-DPLATFORM=byt-codec\;-DSSP_NUM=0"
	"sof-byt-codec\;sof-byt-rt5651-ssp0\;-DCODEC=RT5651\;-DPLATFORM=byt-codec\;-DSSP_NUM=0"
	"sof-byt-codec\;sof-byt-rt5670-ssp0\;-DCODEC=RT5670\;-DPLATFORM=byt-codec\;-DSSP_NUM=0"
	"sof-byt-codec\;sof-byt-rt5682-ssp0\;-DCODEC=RT5682\;-DPLATFORM=byt-codec\;-DSSP_NUM=0"
	"sof-byt-codec\;sof-byt-da7213-ssp0\;-DCODEC=DA7213\;-DPLATFORM=byt-codec\;-DSSP_NUM=0"
	"sof-byt-codec\;sof-byt-cx2072x-ssp0\;-DCODEC=CX2072X\;-DPLATFORM=byt-codec\;-DSSP_NUM=0"
	"sof-byt-codec\;sof-byt-es8316-ssp0\;-DCODEC=ES8316\;-DPLATFORM=byt-codec\;-DSSP_NUM=0"
	"sof-byt-codec\;sof-cht-rt5640\;-DCODEC=RT5640\;-DPLATFORM=cht-codec\;-DSSP_NUM=2"
	"sof-byt-codec\;sof-cht-rt5645\;-DCODEC=RT5645\;-DPLATFORM=cht-codec\;-DSSP_NUM=2"
	"sof-byt-codec\;sof-cht-rt5651\;-DCODEC=RT5651\;-DPLATFORM=cht-codec\;-DSSP_NUM=2"
	"sof-byt-codec\;sof-cht-rt5670\;-DCODEC=RT5670\;-DPLATFORM=cht-codec\;-DSSP_NUM=2"
	"sof-byt-codec\;sof-cht-rt5682\;-DCODEC=RT5682\;-DPLATFORM=cht-codec\;-DSSP_NUM=2"
	"sof-byt-codec\;sof-cht-da7213\;-DCODEC=DA7213\;-DPLATFORM=cht-codec\;-DSSP_NUM=2"
	"sof-byt-codec\;sof-cht-cx2072x\;-DCODEC=CX2072X\;-DPLATFORM=cht-codec\;-DSSP_NUM=2"
	"sof-byt-codec\;sof-cht-es8316\;-DCODEC=ES8316\;-DPLATFORM=cht-codec\;-DSSP_NUM=2"
	"sof-byt-codec\;sof-cht-nau8824\;-DCODEC=NAU8824\;-DPLATFORM=cht-codec\;-DSSP_NUM=2"
	"sof-cht-max98090\;sof-cht-max98090\;-DCODEC=MAX98090\;-DPLATFORM=cht-codec\;-DSSP_NUM=2"
	"sof-cnl-rt274\;sof-cnl-rt274"
	"sof-apl-tdf8532\;sof-apl-tdf8532"
	"sof-apl-pcm512x\;sof-apl-pcm512x\;-DFSYNC=48000"
	"sof-apl-pcm512x\;sof-apl-pcm512x-master\;-DCODEC_MASTER\;-DFSYNC=48000"
	"sof-apl-pcm512x\;sof-apl-pcm512x-master-44100\;-DCODEC_MASTER\;-DFSYNC=44100"
	"sof-apl-pcm512x\;sof-apl-pcm512x-tdfb_28mm-4ch\;-DFSYNC=48000\;-DCHANNELS=4\;-DDMIC16KPROC=tdfb-eq-iir-volume\;-DDMIC16KPROC_FILTER1=tdfb/coef_line4_28mm_pm10deg_16khz.m4\;-DDMICPROC=tdfb-eq-iir-volume\;-DDMICPROC_FILTER1=tdfb/coef_line4_28mm_pm10deg_48khz.m4"
	"sof-apl-pcm512x-nohdmi\;sof-apl-pcm512x-nohdmi\;-DPPROC=volume"
	"sof-apl-demux-pcm512x\;sof-apl-demux-pcm512x"
	"sof-apl-rt298\;sof-apl-rt298"
	"sof-apl-wm8804\;sof-apl-wm8804"
	"sof-apl-da7219\;sof-apl-da7219"
	"sof-glk-da7219-kwd\;sof-glk-da7219-kwd\;-DPLATFORM=glk"
	"sof-glk-da7219-kwd\;sof-cml-da7219-max98357a\;-DPLATFORM=cml"
	"sof-glk-da7219\;sof-glk-da7219"
	"sof-glk-da7219\;sof-glk-eq-da7219\;-DDMICPROC=eq-iir-volume"
	"sof-glk-rt5682\;sof-glk-rt5682"
	"sof-icl-nocodec\;sof-icl-nocodec\;-DPLATFORM=icl"
	"sof-icl-nocodec\;sof-jsl-nocodec\;-DPLATFORM=jsl"
	"sof-apl-pcm512x-nohdmi\;sof-apl-eq-pcm512x\;-DPPROC=eq-iir-eq-fir-volume"
	"sof-apl-dmic\;sof-apl-eq-dmic\;-DCHANNELS=4\;-DCPROC=eq-iir-volume"
	"sof-apl-src-dmic\;sof-apl-src-dmic"
	"sof-apl-dmic-asymmetric\;sof-apl-dmic-a2ch-b4ch\;-DDMICSETTING=apl-dmic-a2b4"
	"sof-apl-dmic-asymmetric\;sof-apl-dmic-a2ch-b2ch\;-DDMICSETTING=apl-dmic-a2b2"
	"sof-apl-dmic-asymmetric\;sof-apl-dmic-a4ch-b2ch\;-DDMICSETTING=apl-dmic-a4b2"
	"sof-apl-dmic-a96k-b16k\;sof-apl-dmic-a96k-b16k"
	"sof-apl-dmic\;sof-apl-dmic-4ch\;-DCHANNELS=4\;-DCPROC=volume"
	"sof-apl-dmic\;sof-apl-dmic-2ch\;-DCHANNELS=2\;-DCPROC=volume"
	"sof-apl-src-50khz-pcm512x\;sof-apl-src-50khz-pcm512x"
	"sof-icl-dmic-4ch\;sof-icl-dmic-4ch"
	"sof-icl-rt700\;sof-icl-rt700\;-DPLATFORM=icl"
	"sof-cml-rt700\;sof-cml-rt700\;-DPLATFORM=cml"
	"sof-icl-rt711-rt1308-rt715-hdmi\;sof-cml-rt711-rt1308-mono-rt715\;-DPLATFORM=cml\;-DMONO"
	"sof-icl-rt711-rt1308-rt715-hdmi\;sof-cml-rt711-rt1308-rt715\;-DPLATFORM=cml"
	"sof-icl-rt711-rt1308-rt715-hdmi\;sof-icl-rt711-rt1308-rt715\;-DPLATFORM=icl"
	"sof-icl-rt711-rt1308-rt715-hdmi\;sof-tgl-rt711-rt1308-mono-rt715\;-DPLATFORM=tgl\;-DMONO"
	"sof-tgl-rt715-rt711-rt1308-hdmi\;sof-tgl-rt715-rt711-rt1308-mono\;-DPLATFORM=tgl"
	"sof-icl-rt711-rt1308-rt715-hdmi\;sof-tgl-rt711-rt1308-rt715\;-DPLATFORM=tgl"
	"sof-icl-rt711-rt1308-rt715-hdmi\;sof-tgl-rt711-rt1316-rt714\;-DPLATFORM=tgl"
	"sof-cnl-rt5682-sdw2\;sof-cnl-rt5682-sdw2\;-DPLATFORM=cnl"
	"sof-apl-asrc-wm8804\;sof-apl-asrc-wm8804"
	"sof-apl-asrc-pcm512x\;sof-apl-asrc-pcm512x"
	"sof-apl-src-pcm512x\;sof-apl-src-pcm512x"
	"sof-cml-rt5682\;sof-cml-rt5682\;-DPLATFORM=cml\;-DDMICPROC=eq-iir-volume\;-DDMIC16KPROC=eq-iir-volume"
	"sof-cml-rt5682\;sof-cml-eq-fir-rt5682\;-DPLATFORM=cml\;-DHSMICPROC=eq-fir-volume\;-DDMICPROC=eq-iir-volume\;-DDMIC16KPROC=eq-iir-volume"
	"sof-cml-rt5682\;sof-cml-eq-fir-loud-rt5682\;-DPLATFORM=cml\;-DHSEARPROC=eq-iir-volume\;-DPIPELINE_FILTER1=eq_iir_coef_loudness.m4\;-DHSMICPROC=eq-fir-volume\;-DPIPELINE_FILTER2=eq_fir_coef_loudness.m4\;-DDMICPROC=eq-iir-volume\;-DDMIC16KPROC=eq-iir-volume"
	"sof-cml-rt5682\;sof-cml-eq-iir-rt5682\;-DPLATFORM=cml\;-DHSEARPROC=eq-iir-volume\;-DDMICPROC=eq-iir-volume\;-DDMIC16KPROC=eq-iir-volume"
	"sof-cml-rt5682\;sof-whl-rt5682\;-DPLATFORM=whl\;-DDMICPROC=eq-iir-volume\;-DDMIC16KPROC=eq-iir-volume"
	"sof-cml-rt5682\;sof-icl-rt5682\;-DPLATFORM=icl\;-DDMICPROC=eq-iir-volume\;-DDMIC16KPROC=eq-iir-volume"
	"sof-cml-rt5682-kwd\;sof-cml-rt5682-kwd\;-DPLATFORM=cml"
	"sof-cml-rt5682-kwd\;sof-whl-rt5682-kwd\;-DPLATFORM=whl"
	"sof-cml-rt5682-kwd\;sof-icl-rt5682-kwd\;-DPLATFORM=icl"
	"sof-cml-src-rt5682\;sof-cml-src-rt5682"
	"sof-cml-demux-rt5682\;sof-cml-demux-rt5682\;-DPLATFORM=cml"
	"sof-cml-demux-rt5682\;sof-whl-demux-rt5682\;-DPLATFORM=whl"
	"sof-cnl-nocodec\;sof-cnl-nocodec"
	"sof-cml-rt5682-max98357a\;sof-cml-rt5682-max98357a\;-DPLATFORM=cml"
	"sof-cml-demux-rt5682-max98357a\;sof-cml-demux-rt5682-max98357a\;-DPLATFORM=cml"
	"sof-cml-rt1011-rt5682\;sof-cml-rt1011-rt5682\;-DPLATFORM=cml\;-DPPROC=volume"
	"sof-cml-rt1011-rt5682\;sof-cml-eq-rt1011-rt5682\;-DPLATFORM=cml\;-DPPROC=eq-iir-eq-fir-volume"
	"sof-tgl-nocodec\;sof-tgl-nocodec"
	"sof-tgl-rt711-i2s-rt1308\;sof-tgl-rt711-i2s-rt1308-2ch\;-DHDMI=1\;-DCHANNELS=2"
	"sof-tgl-rt711-i2s-rt1308\;sof-tgl-rt711-i2s-rt1308-4ch\;-DHDMI=1\;-DCHANNELS=4"
	"sof-tgl-rt711-rt1308\;sof-tgl-rt711-rt1308-2ch\;-DCHANNELS=2"
	"sof-tgl-rt711-rt1308\;sof-tgl-rt711-rt1308-4ch\;-DCHANNELS=4"
	"sof-tgl-rt5682\;sof-tgl-rt5682\;-DHDMI=1"
	"sof-tgl-nocodec\;sof-ehl-nocodec"
	"sof-ehl-rt5660\;sof-ehl-rt5660\;-DHDMI=1"
	"sof-ehl-rt5660\;sof-ehl-rt5660-nohdmi"
	"sof-imx8qxp-nocodec\;sof-imx8qxp-nocodec"
	"sof-imx8qxp-cs42888\;sof-imx8qxp-cs42888"
	"sof-imx8qxp-nocodec-sai\;sof-imx8qxp-nocodec-sai"
	"sof-imx8qxp-wm8960\;sof-imx8qxp-wm8960"
	"sof-tgl-max98357a-rt5682\;sof-tgl-max98357a-rt5682\;-DCODEC=MAX98357A\;-DFMT=s16le"
	"sof-tgl-max98357a-rt5682\;sof-tgl-rt1011-rt5682\;-DCODEC=RT1011\;-DFMT=s24le"
	"sof-tgl-max98373-rt5682\;sof-tgl-max98373-rt5682\;-DAMP_SSP=1"
	"sof-tgl-max98373-rt5682\;sof-tgl-rt5682-ssp0-max98373-ssp2\;-DAMP_SSP=2"
	"sof-tgl-sdw-max98373-rt5682\;sof-tgl-sdw-max98373-rt5682-2ch\;-DCHANNELS=2"
	"sof-tgl-sdw-max98373-rt5682\;sof-tgl-sdw-max98373-rt5682-4ch\;-DCHANNELS=4"
	"sof-jsl-da7219\;sof-jsl-da7219\;-DPLATFORM=jsl"
	"sof-jsl-da7219\;sof-jsl-da7219-mx98360a\;-DPLATFORM=jsl-dedede"
	"sof-imx8mp-wm8960\;sof-imx8mp-wm8960"
	"sof-smart-amplifier-nocodec\;sof-smart-amplifier-nocodec"
	"sof-jsl-rt5682\;sof-jsl-rt5682-rt1015\;-DPLATFORM=jsl-rt1015"
	"sof-jsl-rt5682\;sof-jsl-rt5682-mx98360a\;-DPLATFORM=jsl-dedede"
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
		COMMAND m4 --fatal-warnings
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

# This use of VERBOSE relies on original CMake behavior.
# From the add_custom_command() manual:
#
#    Use of VERBATIM is recommended as it enables correct behavior.
#    When VERBATIM is not given the behavior is platform specific because
#    there is no protection of tool-specific special characters.
#
# This is fine because:
# - We don't expect alsatplg to work on Windows any time soon.
# - CMake is too afraid to remove the original, no-VERBATIM behavior:
#      https://gitlab.kitware.com/cmake/cmake/issues/18849
#
# Also note that in alsa-utils commit v1.2.2~15-gcbabe7a3f0cc, alsatplg
# (accidentally?) started ignoring the verbosity level, now it's just
# verbose or not.
	add_custom_command(
		OUTPUT ${output}.tplg
		COMMAND alsatplg \$\${VERBOSE:+-v 1} -c ${output}.conf -o ${output}.tplg
		DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${output}.conf
		USES_TERMINAL
	)

	add_custom_target(topology_${output} DEPENDS ${output}.tplg)
	add_dependencies(topologies topology_${output})
endforeach()

add_subdirectory(development)
