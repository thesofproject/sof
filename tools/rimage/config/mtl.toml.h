#include "platform-mtl.toml"

	[[module.entry]]
	name = "BRNGUP"
	uuid = "2B79E4F3-4675-F649-89DF-3BC194A91AEB"
	affinity_mask = "0x1"
	instance_count = "1"
	domain_types = "0"
	load_type = "0"
	module_type = "0"
	auto_start = "0"

	index = __COUNTER__

#if CONFIG_COLD_STORE_EXECUTE_DRAM
	[[module.entry]]
	name = "COLD"
	uuid = "D406D134-C3C1-402C-8AEC-6821C0C2B0E6"
	affinity_mask = "3"
	instance_count = "1"
	domain_types = "0"
	load_type = "0"
	module_type = "0"
	auto_start = "0"

	index = __COUNTER__
#endif

	[[module.entry]]
	name = "BASEFW"
	uuid = "0E398C32-5ADE-BA4B-93B1-C50432280EE4"
	affinity_mask = "3"
	instance_count = "1"
	domain_types = "0"
	load_type = "0"
	module_type = "0"
	auto_start = "0"

	index = __COUNTER__

#if defined(CONFIG_COMP_MIXIN_MIXOUT) || defined(LLEXT_FORCE_ALL_MODULAR)
#include <audio/mixin_mixout/mixin_mixout.toml>
#endif

#if defined(CONFIG_COMP_COPIER) || defined(LLEXT_FORCE_ALL_MODULAR)
#include <audio/copier/copier.toml>
#endif

#if defined(CONFIG_COMP_VOLUME) || defined(LLEXT_FORCE_ALL_MODULAR)
#include <audio/volume/volume.toml>
#endif

#if defined(CONFIG_COMP_ASRC) || defined(LLEXT_FORCE_ALL_MODULAR)
#include <audio/asrc/asrc.toml>
#endif

#if defined(CONFIG_COMP_SRC) || defined(LLEXT_FORCE_ALL_MODULAR)
#include <audio/src/src.toml>
#endif

#if defined(CONFIG_COMP_SEL) || defined(LLEXT_FORCE_ALL_MODULAR)
#include <audio/selector/selector.toml>
#endif

#if defined(CONFIG_COMP_UP_DOWN_MIXER) || defined(LLEXT_FORCE_ALL_MODULAR)
#include <audio/up_down_mixer/up_down_mixer.toml>
#endif

#ifdef CONFIG_PROBE
#include <probe/probe.toml>
#endif

#if defined(CONFIG_COMP_MUX) || defined(LLEXT_FORCE_ALL_MODULAR)
#include <audio/mux/mux.toml>
#endif

#ifdef CONFIG_SAMPLE_KEYPHRASE
#include <samples/audio/detect_test.toml>
#endif

#if defined(CONFIG_COMP_KPB) || defined(LLEXT_FORCE_ALL_MODULAR)
#include <audio/kpb.toml>
#endif

#if defined(CONFIG_SAMPLE_SMART_AMP) || defined(LLEXT_FORCE_ALL_MODULAR)
#include <samples/audio/smart_amp_test.toml>
#endif

#if defined(CONFIG_COMP_IIR) || defined(LLEXT_FORCE_ALL_MODULAR)
#include <audio/eq_iir/eq_iir.toml>
#endif

#if defined(CONFIG_COMP_FIR) || defined(LLEXT_FORCE_ALL_MODULAR)
#include <audio/eq_fir/eq_fir.toml>
#endif

#if defined(CONFIG_COMP_ARIA) || defined(LLEXT_FORCE_ALL_MODULAR)
#include <audio/aria/aria.toml>
#endif

#if defined(CONFIG_COMP_DRC) || defined(LLEXT_FORCE_ALL_MODULAR)
#include <audio/drc/drc.toml>
#endif

#if defined(CONFIG_COMP_CROSSOVER) || defined(LLEXT_FORCE_ALL_MODULAR)
#include <audio/crossover/crossover.toml>
#endif

#if defined(CONFIG_COMP_MULTIBAND_DRC) || defined(LLEXT_FORCE_ALL_MODULAR)
#include <audio/multiband_drc/multiband_drc.toml>
#endif

#if defined(CONFIG_COMP_DCBLOCK) || defined(LLEXT_FORCE_ALL_MODULAR)
#include <audio/dcblock/dcblock.toml>
#endif

#if defined(CONFIG_COMP_GOOGLE_RTC_AUDIO_PROCESSING) || defined(LLEXT_FORCE_ALL_MODULAR)
#include <audio/google/google_rtc_audio_processing.toml>
#endif

#if defined(CONFIG_COMP_GOOGLE_CTC_AUDIO_PROCESSING) || defined(LLEXT_FORCE_ALL_MODULAR)
#include <audio/google/google_ctc_audio_processing.toml>
#endif

#if defined(CONFIG_COMP_TDFB) || defined(LLEXT_FORCE_ALL_MODULAR)
#include <audio/tdfb/tdfb.toml>
#endif

#if defined(CONFIG_DTS_CODEC) || defined(LLEXT_FORCE_ALL_MODULAR)
#include <audio/codec/dts/dts.toml>
#endif

#ifdef CONFIG_CADENCE_CODEC
#include <audio/module_adapter/module/cadence.toml>
#endif

#if defined(CONFIG_WAVES_CODEC) || defined(LLEXT_FORCE_ALL_MODULAR)
#include <audio/module_adapter/module/waves/waves.toml>
#endif

#if defined(CONFIG_COMP_RTNR) || defined(LLEXT_FORCE_ALL_MODULAR)
#include <audio/rtnr/rtnr.toml>
#endif

#if defined(CONFIG_COMP_IGO_NR) || defined(LLEXT_FORCE_ALL_MODULAR)
#include <audio/igo_nr/igo_nr.toml>
#endif

#if defined(CONFIG_COMP_MFCC) || defined(LLEXT_FORCE_ALL_MODULAR)
#include <audio/mfcc/mfcc.toml>
#endif

[module]
count = __COUNTER__
