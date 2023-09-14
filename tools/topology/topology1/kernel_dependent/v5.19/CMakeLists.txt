# SPDX-License-Identifier: BSD-3-Clause

# Those topologies are built from topology in the parent directory
set(TPLGS_UP
	## HDaudio codec topologies
	"sof-hda-generic\;sof-hda-generic\;-DDEEP_BUFFER\;-DCHANNELS=0\;-DHSPROC=volume\;-DDYNAMIC=1"
	"sof-hda-generic\;sof-hda-generic-1ch\;-DDEEP_BUFFER\;-DCHANNELS=2\;-DHSPROC=volume\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4\;-DDYNAMIC=1"
	"sof-hda-generic\;sof-hda-generic-1ch-pdm1\;-DPDM1\;-DDEEP_BUFFER\;-DCHANNELS=2\;-DHSPROC=volume\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4\;-DDYNAMIC=1"
	"sof-hda-generic\;sof-hda-generic-2ch\;-DDEEP_BUFFER\;-DCHANNELS=2\;-DHSPROC=volume\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4\;-DDYNAMIC=1"
	"sof-hda-generic\;sof-hda-generic-2ch-pdm1\;-DPDM1\;-DDEEP_BUFFER\;-DCHANNELS=2\;-DHSPROC=volume\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4\;-DDYNAMIC=1"
	"sof-hda-generic\;sof-hda-generic-3ch\;-DDEEP_BUFFER\;-DCHANNELS=4\;-DHSPROC=volume\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4\;-DDYNAMIC=1"
	"sof-hda-generic\;sof-hda-generic-4ch\;-DDEEP_BUFFER\;-DCHANNELS=4\;-DHSPROC=volume\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4\;-DDYNAMIC=1"
	## end HDaudio codec topologies

	## SoundWire topologies
	"sof-icl-rt711-rt1308-rt715-hdmi\;sof-tgl-rt711-rt1308-mono-rt715\;-DHEADSET_DEEP_BUFFER\;-DDYNAMIC=1\;-DPLATFORM=tgl\;-DMONO\;-DUAJ_LINK=0\;-DAMP_1_LINK=1\;-DMIC_LINK=3"
	"sof-icl-rt711-rt1308-rt715-hdmi\;sof-tgl-rt711-l0-rt1316-l1-mono-rt714-l3\;-DHEADSET_DEEP_BUFFER\;-DDYNAMIC=1\;-DPLATFORM=tgl\;-DMONO\;-DUAJ_LINK=0\;-DAMP_1_LINK=1\;-DEXT_AMP_REF\;-DMIC_LINK=3"
	"sof-icl-rt711-rt1308-rt715-hdmi\;sof-tgl-rt715-rt711-rt1308-mono\;-DHEADSET_DEEP_BUFFER\;-DDYNAMIC=1\;-DPLATFORM=tgl\;-DMONO\;-DUAJ_LINK=1\;-DAMP_1_LINK=2\;-DMIC_LINK=0"
	"sof-icl-rt711-rt1308-rt715-hdmi\;sof-tgl-rt711-rt1308-rt715\;-DHEADSET_DEEP_BUFFER\;-DDYNAMIC=1\;-DPLATFORM=tgl\;-DUAJ_LINK=0\;-DAMP_1_LINK=1\;-DAMP_2_LINK=2\;-DMIC_LINK=3"
	"sof-icl-rt711-rt1308-rt715-hdmi\;sof-tgl-rt711-rt1316-rt714\;-DHEADSET_DEEP_BUFFER\;-DDYNAMIC=1\;-DPLATFORM=tgl\;-DUAJ_LINK=0\;-DAMP_1_LINK=1\;-DAMP_2_LINK=2\;-DEXT_AMP_REF\;-DMIC_LINK=3"
	"sof-icl-rt711-rt1308-rt715-hdmi\;sof-adl-rt711-l0-rt1308-l12-rt715-l3\;-DHEADSET_DEEP_BUFFER\;-DDYNAMIC=1\;-DPLATFORM=adl\;-DUAJ_LINK=0\;-DAMP_1_LINK=1\;-DAMP_2_LINK=2\;-DMIC_LINK=3"
	"sof-icl-rt711-rt1308-rt715-hdmi\;sof-adl-rt711-l0-rt1316-l12-rt714-l3\;-DHEADSET_DEEP_BUFFER\;-DDYNAMIC=1\;-DPLATFORM=adl\;-DUAJ_LINK=0\;-DAMP_1_LINK=1\;-DAMP_2_LINK=2\;-DEXT_AMP_REF\;-DMIC_LINK=3"
	"sof-icl-rt711-rt1308-rt715-hdmi\;sof-adl-rt711-l0-rt1316-l13-rt714-l2\;-DHEADSET_DEEP_BUFFER\;-DDYNAMIC=1\;-DPLATFORM=adl\;-DUAJ_LINK=0\;-DAMP_1_LINK=1\;-DAMP_2_LINK=3\;-DEXT_AMP_REF\;-DMIC_LINK=2"
	"sof-icl-rt711-rt1308-rt715-hdmi\;sof-adl-rt711-l2-rt1316-l01-rt714-l3\;-DHEADSET_DEEP_BUFFER\;-DDYNAMIC=1\;-DPLATFORM=adl\;-DUAJ_LINK=2\;-DAMP_1_LINK=0\;-DAMP_2_LINK=1\;-DEXT_AMP_REF\;-DMIC_LINK=3"
	"sof-icl-rt711-rt1308-rt715-hdmi\;sof-adl-rt711-l2-rt1316-l01\;-DHEADSET_DEEP_BUFFER\;-DDYNAMIC=1\;-DPLATFORM=adl\;-DUAJ_LINK=2\;-DAMP_1_LINK=0\;-DAMP_2_LINK=1\;-DEXT_AMP_REF\;-DNO_LOCAL_MIC"
	"sof-icl-rt711-rt1308-rt715-hdmi\;sof-rpl-rt711-l2-rt1316-l01-rt714-l3\;-DHEADSET_DEEP_BUFFER\;-DDYNAMIC=1\;-DPLATFORM=rpl\;-DUAJ_LINK=2\;-DAMP_1_LINK=0\;-DAMP_2_LINK=1\;-DEXT_AMP_REF\;-DMIC_LINK=3"
	"sof-icl-rt711-rt1308-rt715-hdmi\;sof-rpl-rt711-l2-rt1316-l01\;-DHEADSET_DEEP_BUFFER\;-DDYNAMIC=1\;-DPLATFORM=rpl\;-DUAJ_LINK=2\;-DAMP_1_LINK=0\;-DAMP_2_LINK=1\;-DEXT_AMP_REF\;-DNO_LOCAL_MIC"
	"sof-icl-rt711-rt1308-rt715-hdmi\;sof-rpl-rt711-l0-rt1316-l12-rt714-l3\;-DHEADSET_DEEP_BUFFER\;-DDYNAMIC=1\;-DPLATFORM=rpl\;-DUAJ_LINK=0\;-DAMP_1_LINK=1\;-DAMP_2_LINK=2\;-DEXT_AMP_REF\;-DMIC_LINK=3"
	"sof-icl-rt711-rt1308-rt715-hdmi\;sof-rpl-rt711-l0-rt1316-l12\;-DHEADSET_DEEP_BUFFER\;-DDYNAMIC=1\;-DPLATFORM=rpl\;-DUAJ_LINK=0\;-DAMP_1_LINK=1\;-DAMP_2_LINK=2\;-DEXT_AMP_REF\;-DNO_LOCAL_MIC"
	"sof-icl-rt711-rt1308-rt715-hdmi\;sof-rpl-rt711-l0-rt1318-l12-rt714-l3\;-DHEADSET_DEEP_BUFFER\;-DDYNAMIC=1\;-DPLATFORM=rpl\;-DUAJ_LINK=0\;-DAMP_1_LINK=1\;-DAMP_2_LINK=2\;-DEXT_AMP_REF\;-DMIC_LINK=3"
	"sof-icl-rt711-rt1308-rt715-hdmi\;sof-rpl-rt711-l0-rt1318-l12\;-DHEADSET_DEEP_BUFFER\;-DDYNAMIC=1\;-DPLATFORM=rpl\;-DUAJ_LINK=0\;-DAMP_1_LINK=1\;-DAMP_2_LINK=2\;-DEXT_AMP_REF\;-DNO_LOCAL_MIC"
	"sof-tgl-rt711-rt1308\;sof-tgl-rt711-rt1308-2ch\;-DHEADSET_DEEP_BUFFER\;-DDYNAMIC=1\;-DCHANNELS=2\;-DEXT_AMP\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4\;-DPLATFORM=tgl"
	"sof-tgl-rt711-rt1308\;sof-tgl-rt711-rt1308-4ch\;-DHEADSET_DEEP_BUFFER\;-DDYNAMIC=1\;-DCHANNELS=4\;-DEXT_AMP\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4\;-DPLATFORM=tgl"
	"sof-tgl-rt711-rt1308\;sof-tgl-rt711-4ch\;-DHEADSET_DEEP_BUFFER\;-DDYNAMIC=1\;-DCHANNELS=4\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4\;-DPLATFORM=tgl"
	"sof-tgl-rt711-rt1308\;sof-adl-rt711-4ch\;-DHEADSET_DEEP_BUFFER\;-DDYNAMIC=1\;-DCHANNELS=4\;-DBT_OFFLOAD\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4\;-DPLATFORM=adl"
	"sof-tgl-rt711-rt1308\;sof-adl-rt711\;-DHEADSET_DEEP_BUFFER\;-DDYNAMIC=1\;-DCHANNELS=0\;-DBT_OFFLOAD\;-DPLATFORM=adl"
	"sof-tgl-rt711-rt1308\;sof-adl-rt711-l0-rt1316-l3-2ch\;-DHEADSET_DEEP_BUFFER\;-DDYNAMIC=1\;-DCHANNELS=2\;-DEXT_AMP\;-DEXT_AMP_REF\;-DAMP_1_LINK=3\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4\;-DPLATFORM=adl"
	"sof-tgl-rt711-rt1308\;sof-adl-rt711-l0-rt1316-l2-2ch\;-DHEADSET_DEEP_BUFFER\;-DDYNAMIC=1\;-DCHANNELS=2\;-DEXT_AMP\;-DEXT_AMP_REF\;-DAMP_1_LINK=2\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4\;-DPLATFORM=adl"
	"sof-tgl-rt711-rt1308\;sof-adl-rt711-l0-rt1316-l2-4ch\;-DHEADSET_DEEP_BUFFER\;-DDYNAMIC=1\;-DCHANNELS=4\;-DEXT_AMP\;-DEXT_AMP_REF\;-DAMP_1_LINK=2\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4\;-DPLATFORM=adl"

	"sof-tgl-rt711-rt1308\;sof-rpl-rt711-4ch\;-DHEADSET_DEEP_BUFFER\;-DDYNAMIC=1\;-DCHANNELS=4\;-DBT_OFFLOAD\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4\;-DPLATFORM=rpl"
	"sof-tgl-rt711-rt1308\;sof-rpl-rt711\;-DHEADSET_DEEP_BUFFER\;-DDYNAMIC=1\;-DCHANNELS=0\;-DBT_OFFLOAD\;-DPLATFORM=rpl"
	## end SoundWire topologies

	## SoundWire NOJACK topologies
	"sof-icl-rt711-rt1308-rt715-hdmi\;sof-adl-rt1316-l1-mono-rt714-l0\;-DDYNAMIC=1\;-DPLATFORM=adl\;-DMONO\;-DNOJACK\;-DAMP_1_LINK=1\;-DEXT_AMP_REF\;-DMIC_LINK=0"
	"sof-icl-rt711-rt1308-rt715-hdmi\;sof-adl-rt1316-l2-mono-rt714-l0\;-DDYNAMIC=1\;-DPLATFORM=adl\;-DMONO\;-DNOJACK\;-DAMP_1_LINK=2\;-DEXT_AMP_REF\;-DMIC_LINK=0"
	"sof-icl-rt711-rt1308-rt715-hdmi\;sof-adl-rt1316-l12-rt714-l0\;-DDYNAMIC=1\;-DPLATFORM=adl\;-DNOJACK\;-DAMP_1_LINK=1\;-DAMP_2_LINK=2\;-DEXT_AMP_REF\;-DMIC_LINK=0"
	"sof-icl-rt711-rt1308-rt715-hdmi\;sof-adl-rt1316-l2-mono-rt714-l3\;-DDYNAMIC=1\;-DPLATFORM=adl\;-DMONO\;-DNOJACK\;-DAMP_1_LINK=2\;-DEXT_AMP_REF\;-DMIC_LINK=3"
	"sof-icl-rt711-rt1308-rt715-hdmi\;sof-rpl-rt1316-l12-rt714-l0\;-DDYNAMIC=1\;-DPLATFORM=rpl\;-DNOJACK\;-DAMP_1_LINK=1\;-DAMP_2_LINK=2\;-DEXT_AMP_REF\;-DMIC_LINK=0"
	## end SoundWire NOJACK topologies
)

add_custom_target(kernel_dependent_v5_19_topologies1)
add_dependencies(topologies1 kernel_dependent_v5_19_topologies1)

# handle topologies in parent directory
foreach(tplg ${TPLGS_UP})
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
			-I ${CMAKE_CURRENT_SOURCE_DIR}/../../m4
			-I ${CMAKE_CURRENT_SOURCE_DIR}/../../common
			-I ${CMAKE_CURRENT_SOURCE_DIR}/../../platform/common
			-I ${CMAKE_CURRENT_SOURCE_DIR}/../../
			-I ${CMAKE_CURRENT_BINARY_DIR}/../../
			${CMAKE_CURRENT_SOURCE_DIR}/../../common/abi.m4
			${CMAKE_CURRENT_SOURCE_DIR}/../../${input}.m4
			> ${output}.conf
		DEPENDS abi_v1 ${CMAKE_BINARY_DIR}/topology/topology1/abi.h
		VERBATIM
		USES_TERMINAL
	)
	add_alsatplg_command(${output}.conf ${output}.tplg)

	add_custom_target(k519_topology_${output} DEPENDS ${output}.tplg)
	add_dependencies(kernel_dependent_v5_19_topologies1 k519_topology_${output})
endforeach()
