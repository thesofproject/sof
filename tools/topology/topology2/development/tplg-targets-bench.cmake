# SPDX-License-Identifier: BSD-3-Clause

#
# Append test topologies for multiple formats
# to TPLGS array.
#

set(sampleformats "16" "24" "32")

set(components
	"asrc"
	"dcblock"
	"drc"
	"drc_multiband"
	"eqiir"
	"eqfir"
	"gain"
	"igo_nr"
	"level_multiplier"
	"micsel"
	"rtnr"
	"sound_dose"
	"src"
	"src_lite"
	"tdfb"
	"template_comp"
)

set(component_parameters
	"BENCH_ASRC_PARAMS=default"
	"BENCH_DCBLOCK_PARAMS=default"
	"BENCH_DRC_PARAMS=enabled"
	"BENCH_DRC_MULTIBAND_PARAMS=default"
	"BENCH_EQIIR_PARAMS=loudness"
	"BENCH_EQFIR_PARAMS=loudness"
	"BENCH_GAIN_PARAMS=default"
	"BENCH_IGO_NR_PARAMS=default"
	"BENCH_LEVEL_MULTIPLIER_PARAMS=default"
	"BENCH_MICSEL_PARAMS=passthrough"
	"BENCH_RTNR_PARAMS=default"
	"BENCH_SOUND_DOSE_PARAMS=default"
	"BENCH_SRC_PARAMS=default"
	"BENCH_SRC_LITE_PARAMS=default"
	"BENCH_TDFB_PARAMS=default"
	"BENCH_TEMPLATE_COMP_PARAMS=default"
)

set(components_s24
	"aria"
)

set(component_parameters_s24
	"BENCH_ARIA_PARAMS=param_2"
)

set(components_s32
	"micsel_multich"
)

set(component_parameters_s32
	"BENCH_MICSEL_PARAMS=default"
)

set (plat "mtl")

# Add components with all sample formats
foreach(sf ${sampleformats})
	foreach(comp bench_param IN ZIP_LISTS components component_parameters)
		set(item "cavs-benchmark-hda\;sof-hda-benchmark-${comp}${sf}\;PLATFORM=${plat},BENCH_MODULE_FORMAT=s${sf},BENCH_CONFIG=${comp}${sf},${bench_param}")
		list(APPEND TPLGS "${item}")
		set(item "cavs-benchmark-sdw\;sof-${plat}-sdw-benchmark-${comp}${sf}-sdw0\;PLATFORM=${plat},BENCH_MODULE_FORMAT=s${sf},BENCH_CONFIG=${comp}${sf},${bench_param}")
		list(APPEND TPLGS "${item}")
		set(item "cavs-benchmark-sdw\;sof-${plat}-sdw-benchmark-${comp}${sf}-simplejack\;PLATFORM=${plat},SDW_JACK_OUT_STREAM=Playback-SimpleJack,SDW_JACK_IN_STREAM=Capture-SimpleJack,BENCH_MODULE_FORMAT=s${sf},BENCH_CONFIG=${comp}${sf},${bench_param}")
		list(APPEND TPLGS "${item}")
#		#message(STATUS "Item=" ${item})
	endforeach()
endforeach()

# Add components with single format
set (sf "24")
foreach(comp bench_param IN ZIP_LISTS components_s24 component_parameters_s24)
	set(item "cavs-benchmark-hda\;sof-hda-benchmark-${comp}${sf}\;PLATFORM=${plat},BENCH_MODULE_FORMAT=s${sf},BENCH_CONFIG=${comp}${sf},${bench_param}")
	list(APPEND TPLGS "${item}")
	set(item "cavs-benchmark-sdw\;sof-${plat}-sdw-benchmark-${comp}${sf}-sdw0\;PLATFORM=${plat},BENCH_MODULE_FORMAT=s${sf},BENCH_CONFIG=${comp}${sf},${bench_param}")
	list(APPEND TPLGS "${item}")
	set(item "cavs-benchmark-sdw\;sof-${plat}-sdw-benchmark-${comp}${sf}-simplejack\;PLATFORM=${plat},SDW_JACK_OUT_STREAM=Playback-SimpleJack,SDW_JACK_IN_STREAM=Capture-SimpleJack,BENCH_MODULE_FORMAT=s${sf},BENCH_CONFIG=${comp}${sf},${bench_param}")
	list(APPEND TPLGS "${item}")
endforeach()

set (sf "32")
foreach(comp bench_param IN ZIP_LISTS components_s32 component_parameters_s32)
	#message(STATUS "Bench_param=" ${bench_param})
	set(item "cavs-benchmark-hda\;sof-hda-benchmark-${comp}${sf}\;PLATFORM=${plat},BENCH_MODULE_FORMAT=s${sf},BENCH_CONFIG=${comp}${sf},${bench_param}")
	list(APPEND TPLGS "${item}")
	set(item "cavs-benchmark-sdw\;sof-${plat}-sdw-benchmark-${comp}${sf}-sdw0\;PLATFORM=${plat},BENCH_MODULE_FORMAT=s${sf},BENCH_CONFIG=${comp}${sf},${bench_param}")
	list(APPEND TPLGS "${item}")
	set(item "cavs-benchmark-sdw\;sof-${plat}-sdw-benchmark-${comp}${sf}-simplejack\;PLATFORM=${plat},SDW_JACK_OUT_STREAM=Playback-SimpleJack,SDW_JACK_IN_STREAM=Capture-SimpleJack,BENCH_MODULE_FORMAT=s${sf},BENCH_CONFIG=${comp}${sf},${bench_param}")
	list(APPEND TPLGS "${item}")
endforeach()
