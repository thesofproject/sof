# SPDX-License-Identifier: BSD-3-Clause

#
# Append test topologies for multiple platforms and multiple formats
# to TPLGS array.
#

set(platforms "TGL" "MTL")
set(sampleformats "16" "24" "32")

set(components
	"dcblock"
	"drc"
	"drc_multiband"
	"eqiir"
	"eqfir"
	"gain"
	"igo_nr"
	"rtnr"
	"src"
	"src_lite"
)

set(component_parameters
	"BENCH_DCBLOCK_PARAMS=default"
	"BENCH_DRC_PARAMS=enabled"
	"BENCH_DRC_MULTIBAND_PARAMS=default"
	"BENCH_EQIIR_PARAMS=highpass_50hz_0db_48khz"
	"BENCH_EQFIR_PARAMS=loudness"
	"BENCH_GAIN_PARAMS=default"
	"BENCH_IGO_NR_PARAMS=default"
	"BENCH_RTNR_PARAMS=default"
	"BENCH_SRC_PARAMS=default"
	"BENCH_SRC_LITE_PARAMS=default"
)

set(components_s32
	"aria"
)

set(component_parameters_s32
	"BENCH_ARIA_PARAMS=default"
)

foreach(p ${platforms})
	string(TOLOWER ${p} pl)

	# Add components with all sample formats
	foreach(sf ${sampleformats})
		foreach(comp bench_param IN ZIP_LISTS components component_parameters)
			    set(item "sof-hda-generic\;sof-hda-benchmark-${comp}${sf}-${pl}\;PLATFORM=${p},HDA_CONFIG=benchmark,BENCH_CONFIG=${comp}${sf},${bench_param}")
			    #message(STATUS "Item=" ${item})
			    list(APPEND TPLGS "${item}")
		endforeach()
	endforeach()

	# Add components with single format
	set (sf "32")
	foreach(comp bench_param IN ZIP_LISTS components_s32 component_parameters_s32)
		set(item "sof-hda-generic\;sof-hda-benchmark-${comp}${sf}-${pl}\;PLATFORM=${p},HDA_CONFIG=benchmark,BENCH_CONFIG=${comp}${sf},${bench_param}")
		#message(STATUS "Item=" ${item})
		list(APPEND TPLGS "${item}")
	endforeach()
endforeach()
