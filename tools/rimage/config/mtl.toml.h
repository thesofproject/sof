version = [3, 0]

[adsp]
name = "mtl"
image_size = "0x2C0000" # (22) bank * 128KB
alias_mask = "0xE0000000"

[[adsp.mem_zone]]
type = "ROM"
base = "0x1FF80000"
size = "0x400"
[[adsp.mem_zone]]
type = "IMR"
base = "0xA104A000"
size = "0x2000"
[[adsp.mem_zone]]
type = "SRAM"
base = "0xa00f0000"
size = "0x100000"

[[adsp.mem_alias]]
type = "uncached"
base = "0x40000000"
[[adsp.mem_alias]]
type = "cached"
base = "0xA0000000"

[cse]
partition_name = "ADSP"
[[cse.entry]]
name = "ADSP.man"
offset = "0x5c"
length = "0x4b8"
[[cse.entry]]
name = "ADSP.met"
offset = "0x4c0"
length = "0x70"
[[cse.entry]]
name = "ADSP"
offset = "0x540"
length = "0x0"  # calculated by rimage

[css]

[signed_pkg]
name = "ADSP"
[[signed_pkg.module]]
name = "ADSP.met"

[adsp_file]
[[adsp_file.comp]]
base_offset = "0x2000"

[fw_desc.header]
name = "ADSPFW"
load_offset = "0x40000"

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

#include <audio/mixin_mixout/mixin_mixout.toml>

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

#ifdef CONFIG_COMP_SRC
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

#ifdef CONFIG_COMP_SRC_LITE
#include <audio/src/src_lite.toml>
#endif

#ifdef CONFIG_CADENCE_CODEC
#include <audio/module_adapter/module/cadence.toml>
#endif

[module]
count = __COUNTER__
