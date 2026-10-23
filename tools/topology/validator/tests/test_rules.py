# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2026 Intel Corporation. All rights reserved.

"""One positive and at least one negative case per rule."""

import struct

import pytest

import checks
from tplgbuild import (Topology, TupleType, in_format, module_tuples, out_format,
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
