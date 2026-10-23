# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2026 Intel Corporation. All rights reserved.

"""One positive and at least one negative case per rule."""

import struct

import pytest

import checks
import tplg_validate
from checks.paths import pcm_paths
from tplgbuild import (Topology, TupleType, in_format, module_tuples, name, out_format,
                       playback_ssp, tuples, words)
from tplgparse import binary, model


def run(data: bytes, num_cores=0):
    tplg = model.build(binary.parse_bytes(data), "test.tplg")
    return checks.run(tplg, checks.Options(num_cores=num_cores))


def rules(findings, severity=None):
    return {f.rule for f in findings if severity is None or f.severity == severity}


def widget(t, wname):
    return next(w for _i, w, _k in t.widgets if binary.decode_name(w.name) == wname)


def test_valid_topology_is_clean():
    assert run(playback_ssp().build()) == []


# --- L1 -----------------------------------------------------------------------

def test_s001_truncated_file():
    data = playback_ssp().build()
    assert "S001" in rules(run(data[:-10]), checks.ERROR)


def test_s001_bad_magic():
    data = bytearray(playback_ssp().build())
    data[0:4] = b"XXXX"
    assert "S001" in rules(run(bytes(data)), checks.ERROR)


def test_s002_vendor_section():
    t = playback_ssp()
    data = t.build() + t._section(binary.TplgType.PDATA, [b"\0" * 8])
    assert "S002" in rules(run(data), checks.ERROR)


def test_s003_widget_size():
    t = playback_ssp()
    widget(t, "gain.1.1").size = 100
    assert "S003" in rules(run(t.build()), checks.ERROR)


def test_s004_unterminated_name():
    t = playback_ssp()
    widget(t, "gain.1.1").sname = b"x" * binary.NAME_LEN
    assert "S004" in rules(run(t.build()), checks.ERROR)


def test_s005_unknown_widget_type():
    t = playback_ssp()
    widget(t, "gain.1.1").id = 99
    assert "S005" in rules(run(t.build()), checks.ERROR)


def test_s007_tlv_type():
    t = playback_ssp()
    t.widgets[2][2][0]["hdr"].tlv.type = 3  # DB_RANGE
    assert "S007" in rules(run(t.build()), checks.ERROR)


def test_s008_unbound_kcontrol_handler():
    t = playback_ssp()
    t.widgets[2][2][0]["hdr"].ops.get = 999
    assert "S008" in rules(run(t.build()), checks.ERROR)


# --- L2 -----------------------------------------------------------------------

def test_t001_too_many_pins():
    t = playback_ssp()
    widget(t, "gain.1.1").priv = module_tuples(in_pins=9)
    assert "T001" in rules(run(t.build()), checks.ERROR)


def test_t001_missing_format_count():
    t = playback_ssp()
    widget(t, "gain.1.1").priv = tuples(words(num_input_pins=1, num_output_pins=1),
                                        (TupleType.UUID, [(405, b"\1" * 16)]),
                                        in_format(), out_format())
    assert "T001" in rules(run(t.build()), checks.ERROR)


def test_t003_bad_array_size():
    t = playback_ssp()
    priv = bytearray(module_tuples())
    struct.pack_into("<I", priv, 0, 9999)
    widget(t, "gain.1.1").priv = bytes(priv)
    assert {"T001", "T003"} <= rules(run(t.build()), checks.ERROR)


def test_t004_unknown_token():
    t = playback_ssp()
    widget(t, "gain.1.1").priv = module_tuples(extra=[(TupleType.WORD, [(31337, 1)])])
    assert "T004" in rules(run(t.build()), checks.WARNING)


def test_t005_word_token_in_string_array():
    t = playback_ssp()
    widget(t, "gain.1.1").priv = module_tuples() + tuples((TupleType.STRING, [(411, "2")]))
    assert "T005" in rules(run(t.build()), checks.ERROR)


def test_t006_ignored_formats():
    t = playback_ssp()
    widget(t, "gain.1.1").priv = module_tuples(n_in=1, ins=[in_format(), in_format(rate=96000)])
    found = run(t.build())
    assert "T006" in rules(found, checks.WARNING)
    assert any("96000Hz" in f.message for f in found if f.rule == "T006")


def test_t006_missing_formats():
    t = playback_ssp()
    widget(t, "gain.1.1").priv = module_tuples(n_in=2, ins=[in_format()])
    assert "T006" in rules(run(t.build()), checks.ERROR)


def test_t006_format_block_missing_token():
    t = playback_ssp()
    first = (TupleType.WORD, [tp for tp in in_format()[1] if tp[0] != 1901])  # no bit depth
    widget(t, "gain.1.1").priv = module_tuples(n_in=2, ins=[first, in_format(depth=16, valid=16)])
    assert "T006" in rules(run(t.build()), checks.ERROR)


# --- L3 -----------------------------------------------------------------------

def test_g001_dangling_route():
    t = playback_ssp()
    t.route("gain.1.1", "no-such-widget", 1)
    assert "G001" in rules(run(t.build()), checks.ERROR)


def test_g004_unconnected_widget():
    t = playback_ssp()
    t.widget("gain.1.2", "pga", 1, module_tuples(),
             kcontrols=[Topology.kcontrol("Vol 2", access=0x13, max=45, num_channels=2)])
    assert "G004" in rules(run(t.build()), checks.WARNING)


def test_g005_widget_without_pipeline():
    t = playback_ssp()
    t.widgets[2] = (7,) + t.widgets[2][1:]  # gain.1.1 into pipeline index 7
    assert "G005" in rules(run(t.build()), checks.ERROR)


def test_g006_chain_dma_mismatch():
    t = playback_ssp()
    t.pipeline(2, chain_dma=1)
    t.widgets[3] = (2,) + t.widgets[3][1:]  # DAI copier into chain DMA pipeline 2
    assert "G006" in rules(run(t.build()), checks.ERROR)


def test_g007_volatile_control_in_dynamic_pipeline():
    t = Topology()
    t.pipeline(1, dynamic=1)
    base = playback_ssp()
    t.widgets += base.widgets[1:]
    t.routes, t.pcms, t.links = base.routes, base.pcms, base.links
    t.widgets[2][2][0]["hdr"].access |= 0x4  # VOLATILE
    assert "G007" in rules(run(t.build()), checks.ERROR)


def test_w001_pga_without_kcontrol():
    t = playback_ssp()
    t.widgets[2] = t.widgets[2][:2] + ([],)
    widget(t, "gain.1.1").num_kcontrols = 0
    assert "W001" in rules(run(t.build()), checks.ERROR)


def test_w002_no_formats():
    t = playback_ssp()
    widget(t, "gain.1.1").priv = module_tuples(n_in=0, n_out=0)
    assert "W002" in rules(run(t.build()), checks.ERROR)


def test_w003_missing_uuid():
    t = playback_ssp()
    widget(t, "gain.1.1").priv = tuples(words(num_input_audio_formats=1,
                                              num_output_audio_formats=1,
                                              num_input_pins=1, num_output_pins=1),
                                        in_format(), out_format())
    assert "W003" in rules(run(t.build()), checks.ERROR)


def test_w004_unknown_dai_type():
    t = playback_ssp()
    widget(t, "dai-copier.SSP.NoCodec-0.playback").priv = module_tuples(
        out_pins=0, extra=[(TupleType.STRING, [(154, "FOO")])])
    assert "W004" in rules(run(t.build()), checks.ERROR)


def test_w005_chain_dma_with_ssp():
    assert "W005" in rules(run(playback_ssp(chain_dma=1).build()), checks.ERROR)


def test_w006_core_id():
    t = playback_ssp()
    widget(t, "gain.1.1").priv = module_tuples(extra=[words(core=3)])
    assert "W006" in rules(run(t.build(), num_cores=2), checks.WARNING)
    assert "W006" not in rules(run(t.build(), num_cores=4))


def test_w007_zero_pins_on_route():
    t = playback_ssp()
    widget(t, "gain.1.1").priv = module_tuples(in_pins=0)
    found = run(t.build())
    assert "W007" in rules(found, checks.WARNING)
    assert any("Port0" in f.message for f in found if f.rule == "W007")


def test_w007_pin_binding_mismatch():
    t = playback_ssp()
    widget(t, "gain.1.1").priv = module_tuples(
        in_pins=2, extra=[(TupleType.STRING, [(413, "foo")]),
                          (TupleType.STRING, [(413, "bar")])])
    assert "W007" in rules(run(t.build()), checks.WARNING)


def test_c001_volume_without_tlv():
    t = playback_ssp()
    t.widgets[2][2][0]["hdr"].access = 0x3
    assert "C001" in rules(run(t.build()), checks.ERROR)


def test_c002_bytes_control_magic():
    t = playback_ssp()
    blob = struct.pack("<IIII16x", 0x12345678, 0, 4, 0) + b"\0" * 4
    k = Topology.kcontrol("Bytes", kind="BYTES", info=5, get=258, put=258, access=0x3,
                          priv=blob, max=64)
    t.widgets[2] = t.widgets[2][:2] + (t.widgets[2][2] + [k],)
    widget(t, "gain.1.1").num_kcontrols = 2
    assert "C002" in rules(run(t.build()), checks.ERROR)


def test_p001_pcm_without_host():
    t = playback_ssp()
    t.pcm("Port1", pcm_id=1, playback="Port1 Playback")
    assert "P001" in rules(run(t.build()), checks.ERROR)


def test_l001_link_without_tokens():
    t = playback_ssp()
    t.links[0].priv = b""
    assert "L001" in rules(run(t.build()), checks.ERROR)


def test_m001_manifest_tlv_overflow():
    t = playback_ssp()
    t.manifest_priv = struct.pack("<HHHH", 3, 30, 0, 1) + struct.pack("<II", 1, 1000)
    assert "M001" in rules(run(t.build()), checks.ERROR)


def test_f002_pcm_format_without_host_format():
    t = playback_ssp()
    widget(t, "host-copier.0.playback").priv = module_tuples(
        n_in=2, in_pins=0, ins=[in_format(), in_format(depth=16, valid=16)])
    t.pcms[0].caps[0].formats = 1 << 10 | 1 << 14  # S32_LE and FLOAT_LE
    found = run(t.build())
    assert "F002" in rules(found, checks.WARNING)
    assert any("FLOAT_LE" in f.message for f in found if f.rule == "F002")


@pytest.mark.parametrize("rule_id", [r.id for r in checks.load_rules()])
def test_every_rule_names_kernel_function(rule_id):
    r = next(r for r in checks.RULES if r.id == rule_id)
    assert r.kernel and r.summary and r.layer in ("L1", "L2", "L3", "L4")


# --- review fixes: malformed input must not hang or crash the validator -------

def test_huge_num_elems_is_bounded():
    t = playback_ssp()
    bogus = struct.pack("<III", 12 + 8, TupleType.WORD, 0xffffffff) + struct.pack("<II", 1, 1)
    widget(t, "host-copier.0.playback").priv = module_tuples(in_pins=0) + bogus
    assert "T003" in rules(run(t.build()), checks.ERROR)


def test_huge_format_count_is_bounded():
    t = playback_ssp()
    widget(t, "gain.1.1").priv = module_tuples(n_in=0xffffffff, ins=[in_format()])
    found = run(t.build())
    assert any(f.rule == "T006" and f.severity == checks.ERROR and "4294967295" in f.message
               for f in found)


def test_huge_pcm_channel_count_is_bounded():
    t = playback_ssp()
    # two host formats, so that F001 walks the PCM caps
    widget(t, "host-copier.0.playback").priv = module_tuples(
        n_in=2, in_pins=0, ins=[in_format(), in_format(depth=16, valid=16)])
    t.pcms[0].caps[0].channels_max = 0xffffffff
    assert "F001" in rules(run(t.build()), checks.INFO)


def test_c001_volume_max_zero_needs_tlv():
    t = playback_ssp()
    k = t.widgets[2][2][0]
    k["hdr"].access = 0x3
    k["body"].max = 0
    assert "C001" in rules(run(t.build()), checks.ERROR)


def test_m001_manifest_without_complete_tlv_count():
    t = playback_ssp()
    t.manifest_priv = struct.pack("<HHH", 3, 30, 0) + b"\1"
    assert "M001" in rules(run(t.build()), checks.ERROR)


def test_t006_two_formats_in_one_vendor_array():
    t = playback_ssp()
    both = (TupleType.WORD, in_format()[1] + in_format(depth=16, valid=16)[1])
    widget(t, "gain.1.1").priv = module_tuples(n_in=2, ins=[both])
    assert "T006" not in rules(run(t.build()))


def test_t006_ignored_format_differing_in_consumed_field():
    t = playback_ssp()
    other_map = (TupleType.WORD, [(tok, 0x3210 if tok == 1904 else v)
                                  for tok, v in in_format()[1]])
    widget(t, "gain.1.1").priv = module_tuples(n_in=1, ins=[in_format(), other_map])
    found = [f for f in run(t.build()) if f.rule == "T006"]
    assert [f.severity for f in found] == [checks.WARNING]
    assert "ch_map 0x3210" in found[0].message


def test_rule_crash_is_reported_as_error():
    def broken(_ctx):
        raise struct.error("unpack requires a buffer of 8 bytes")
        yield  # pylint: disable=unreachable
    checks.load_rules()
    checks.RULES.append(checks.Rule("X999", "L3", checks.ERROR, "none", "test", broken))
    try:
        found = run(playback_ssp().build())
    finally:
        checks.RULES.pop()
    assert any(f.rule == "X999" and "internal error" in f.message for f in found)


# --- route controls and DAPM-only widgets ----------------------------------

def capture_mux(t, entries=("Mic", "Line")):
    "A DAPM-only mux in pipeline index 5, which has no scheduler widget."
    texts = [name(e) for e in entries] + [name("")] * (16 - len(entries))
    enum = Topology.kcontrol("Capture Source", kind="ENUM", info=65, get=65, put=65,
                             access=0x3, items=len(entries), texts=texts, num_channels=1)
    t.widget("Capture Mux", "mux", 5, kcontrols=[enum])
    t.widget("Mic", "input", 5)


def test_g002_mux_route_names_enum_entry():
    t = playback_ssp()
    capture_mux(t)
    t.route("Mic", "Capture Mux", 5, control="Mic")
    assert "G002" not in rules(run(t.build()))


def test_g002_mux_route_unknown_entry():
    t = playback_ssp()
    capture_mux(t)
    t.route("Mic", "Capture Mux", 5, control="Headset")
    assert "G002" in rules(run(t.build()), checks.ERROR)


def test_g002_mixer_route_unknown_kcontrol():
    t = playback_ssp()
    t.widget("mixin.1.1", "mixer", 1, module_tuples())
    t.route("gain.1.1", "mixin.1.1", 1, control="Switch")
    assert "G002" in rules(run(t.build()), checks.ERROR)


def test_g002_control_ignored_for_other_sinks():
    t = playback_ssp()
    t.route("host-copier.0.playback", "gain.1.1", 1, control="Anything")
    assert "G002" not in rules(run(t.build()))


def test_g005_dapm_only_widget_needs_no_pipeline():
    t = playback_ssp()
    capture_mux(t)
    t.route("Mic", "Capture Mux", 5, control="Mic")
    assert "G005" not in rules(run(t.build()))


def test_g001_route_into_base_topology():
    """A feature topology may route to widgets of a function topology that
    the kernel loaded before it into the same component."""
    feature = Topology()
    feature.pipeline(9)
    feature.widget("gain.9.1", "pga", 9, module_tuples(),
                   kcontrols=[Topology.kcontrol("Vol 9", access=0x13, max=45, num_channels=2)])
    feature.route("gain.1.1", "gain.9.1", 9)

    alone = model.build(binary.parse_bytes(feature.build()), "feature.tplg")
    assert "G001" in rules(checks.run(alone, checks.Options()), checks.ERROR)

    on_base = model.build(binary.parse_bytes(feature.build()), "feature.tplg")
    on_base.add_base(model.build(binary.parse_bytes(playback_ssp().build()), "base.tplg"))
    found = checks.run(on_base, checks.Options())
    assert "G001" not in rules(found)
    assert not [f for f in found if "base.tplg" in f.where]


def test_feature_on_feature_and_single_shared_widget():
    """Like sof-sdca-amp-ref on top of sof-sdca-jack-ref-dai: a feature
    topology may use a widget defined by a feature topology loaded before
    it, but must not define it again."""
    def feature(index, defines_shared):
        f = Topology()
        f.pipeline(index)
        f.widget(f"gain.{index}.1", "pga", index, module_tuples(),
                 kcontrols=[Topology.kcontrol(f"Vol {index}", access=0x13, max=45,
                                              num_channels=2)])
        if defines_shared:
            f.widget("shared.copier", "buffer", index, module_tuples())
        f.route("shared.copier", f"gain.{index}.1", index)
        return model.build(binary.parse_bytes(f.build()), f"feature{index}.tplg")

    function = model.build(binary.parse_bytes(playback_ssp().build()), "function.tplg")
    first = feature(8, defines_shared=True)
    first.add_base(function)

    second = feature(9, defines_shared=False)
    second.add_base(first)
    assert "G001" not in rules(checks.run(second, checks.Options()))

    duplicate = feature(9, defines_shared=True)
    duplicate.add_base(first)
    found = checks.run(duplicate, checks.Options())
    assert any(f.rule == "G003" and "base topology" in f.message for f in found)


def pcm_on_base_host():
    "A feature topology with a PCM bound to the host copier of its base."
    feature = Topology()
    feature.pcm("Port0 again", pcm_id=7, playback="Port0 Playback")
    return model.build(binary.parse_bytes(feature.build()), "feature.tplg")


def test_p001_pcm_binds_to_base_host_copier():
    alone = pcm_on_base_host()
    assert "P001" in rules(checks.run(alone, checks.Options()), checks.ERROR)
    on_base = pcm_on_base_host()
    on_base.add_base(model.build(binary.parse_bytes(playback_ssp().build()), "base.tplg"))
    assert "P001" not in rules(checks.run(on_base, checks.Options()))


def test_pcm_path_through_base_host_copier():
    on_base = pcm_on_base_host()
    on_base.add_base(model.build(binary.parse_bytes(playback_ssp().build()), "base.tplg"))
    paths = pcm_paths(checks.Context(on_base, checks.Options()))
    assert [p.host.name for p in paths] == ["host-copier.0.playback"]
    assert "dai-copier.SSP.NoCodec-0.playback" in [w.name for w in paths[0].widgets]


def test_base_chain_does_not_duplicate(tmp_path):
    paths = []
    for i in range(4):
        t = Topology()
        t.pipeline(10 + i)
        path = tmp_path / f"base{i}.tplg"
        path.write_bytes(t.build())
        paths.append(str(path))
    last = tplg_validate.load_bases(paths)
    # one scheduler widget from each of the three earlier bases
    assert sorted(w.name for w in last.base_widgets) == ["pipeline.10", "pipeline.11",
                                                         "pipeline.12"]
