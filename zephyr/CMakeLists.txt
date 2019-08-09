if(CONFIG_SOF)
zephyr_interface_library_named(SOF)

# include redefines incompatible things on src/include
target_include_directories(SOF INTERFACE
	include
	include/platform
	#../src/arch/host/include
	../src/include)

zephyr_library()
zephyr_library_sources(
	# Dummy version, might move these to Zephyr
	alloc.c
	ll_schedule.c
	edf_schedule.c
	schedule.c
	panic.c
	ipc.c
	timer.c
	interrupt.c
	# SOF src/audio
	../src/audio/host.c
	../src/audio/pipeline.c
	#../src/audio/pipeline_static.c
	../src/audio/component.c
	../src/audio/buffer.c
	# SOF src/ipc
	../src/ipc/ipc.c
	../src/ipc/handler.c
	# SOF src/lib
	#../src/lib/lib.c
	#../src/lib/alloc.c
	#../src/lib/ll_schedule.c
	#../src/lib/notifier.c
	#../src/lib/edf_schedule.c
	#../src/lib/schedule.c
	#../src/lib/agent.c
	../src/lib/interrupt.c
	#../src/lib/pm_runtime.c
	#../src/lib/clk.c
	#../src/lib/dma.c
	#../src/lib/dai.c
	#../src/lib/panic.c
	#../src/lib/wait.c
	# SOF src/math
	#../src/math/numbers.c
	../src/math/trig.c
)

zephyr_library_sources_ifdef(CONFIG_SOF_STATIC_PIPELINE pipeline_static.c)
zephyr_library_sources_ifdef(CONFIG_SOF_COMP_DAI ../src/audio/dai.c)
zephyr_library_sources_ifdef(CONFIG_SOF_COMP_TONE ../src/audio/tone.c)
zephyr_library_sources_ifdef(CONFIG_SOF_COMP_MIXER ../src/audio/mixer.c)
zephyr_library_sources_ifdef(CONFIG_SOF_COMP_MUX ../src/audio/mux/mux.c)
zephyr_library_sources_ifdef(CONFIG_SOF_COMP_SWITCH ../src/audio/switch.c)
#zephyr_library_sources_ifdef(CONFIG_SOF_COMP_KPB ../src/audio/kpb.c)
zephyr_library_sources_ifdef(CONFIG_SOF_COMP_TEST_KEYPHRASE
	../src/audio/detect_test.c)
zephyr_library_sources_ifdef(CONFIG_SOF_COMP_VOLUME
	../src/audio/volume/volume_generic.c
	../src/audio/volume/volume_hifi3.c
	../src/audio/volume/volume.c)
zephyr_library_sources_ifdef(CONFIG_SOF_COMP_SRC
	../src/audio/src/src_generic.c
	../src/audio/src/src_hifi2ep.c
	../src/audio/src/src_hifi3.c
	../src/audio/src/src.c)
zephyr_library_sources_ifdef(CONFIG_SOF_COMP_FIR
	../src/audio/eq_fir/eq_fir.c
	../src/audio/eq_fir/fir_hifi2ep.c
	../src/audio/eq_fir/fir_hifi3.c
	../src/audio/eq_fir/fir.c)
zephyr_library_sources_ifdef(CONFIG_SOF_COMP_IIR
	../src/audio/eq_iir/eq_iir.c
	../src/audio/eq_iir/iir.c)
zephyr_library_sources_ifdef(CONFIG_SOF_COMP_SEL
	../src/audio/selector/selector_generic.c
	../src/audio/selector/selector.c)

zephyr_library_link_libraries(SOF)
target_link_libraries(SOF INTERFACE zephyr_interface)
endif()
