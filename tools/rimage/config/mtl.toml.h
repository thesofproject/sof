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

#ifdef CONFIG_COMP_MIXIN_MIXOUT
#include <audio/mixin_mixout/mixin_mixout.toml>
#endif

#ifdef CONFIG_COMP_COPIER
#include <audio/copier/copier.toml>
#endif

#ifdef CONFIG_COMP_PEAK_VOL
#include <audio/volume/peakvol.toml>
#endif

#ifdef CONFIG_COMP_GAIN
#include <audio/volume/gain.toml>
#endif

#ifdef CONFIG_COMP_ASRC
#include <audio/asrc/asrc.toml>
#endif

#ifdef CONFIG_COMP_SRC_LITE
#include <audio/src/src_lite.toml>
#else
#include <audio/src/src.toml>
#endif

#ifdef CONFIG_COMP_SEL
#include <audio/selector/selector.toml>
#endif

#ifdef CONFIG_COMP_UP_DOWN_MIXER
#include <audio/up_down_mixer/up_down_mixer.toml>
#endif

#ifdef CONFIG_PROBE
#include <probe/probe.toml>
#endif

#ifdef CONFIG_COMP_MUX
#include <audio/mux/mux.toml>
#endif

#ifdef CONFIG_SAMPLE_KEYPHRASE
#include <samples/audio/detect_test.toml>
#endif

#ifdef CONFIG_COMP_KPB
#include <audio/kpb.toml>
#endif

#ifdef CONFIG_SAMPLE_SMART_AMP
#include <samples/audio/smart_amp_test.toml>
#endif

#ifdef CONFIG_COMP_IIR
#include <audio/eq_iir/eq_iir.toml>
#endif

#ifdef CONFIG_COMP_FIR
#include <audio/eq_fir/eq_fir.toml>
#endif

#ifdef CONFIG_COMP_ARIA
#include <audio/aria/aria.toml>
#endif

#ifdef CONFIG_COMP_DRC
#include <audio/drc/drc.toml>
#endif

#ifdef CONFIG_COMP_CROSSOVER
#include <audio/crossover/crossover.toml>
#endif

#ifdef CONFIG_COMP_MULTIBAND_DRC
#include <audio/multiband_drc/multiband_drc.toml>
#endif

#ifdef CONFIG_COMP_DCBLOCK
#include <audio/dcblock/dcblock.toml>
#endif

#ifdef CONFIG_COMP_GOOGLE_RTC_AUDIO_PROCESSING
#include <audio/google/google_rtc_audio_processing.toml>
#endif

#ifdef CONFIG_COMP_TDFB
#include <audio/tdfb/tdfb.toml>
#endif

#ifdef CONFIG_DTS_CODEC
#include <audio/codec/dts/dts.toml>
#endif

#ifdef CONFIG_CADENCE_CODEC
#include <audio/module_adapter/module/cadence.toml>
#endif

#ifdef CONFIG_COMP_RTNR
#include <audio/rtnr/rtnr.toml>
#endif

#ifdef CONFIG_COMP_IGO_NR
#include <audio/igo_nr/igo_nr.toml>
#endif

[module]
count = __COUNTER__
