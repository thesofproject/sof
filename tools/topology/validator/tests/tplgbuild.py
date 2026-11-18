# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2026 Intel Corporation. All rights reserved.

"""Build small topology binaries for tests.

Records start from an all-zero instance of the binary.py structure and
get the fields a test cares about, so each test fixture states only what
matters for the rule under test.
"""

import struct

from tplgparse import binary
from tplgparse.binary import TplgType, TupleType

SIZE = binary.SIZEOF


def name(s: str) -> bytes:
    raw = s.encode()
    return raw + b"\0" * (binary.NAME_LEN - len(raw))


def zero(record):
    "All-zero instance of a record whose private data is empty."
    fixed = sum(sc.sizeof() for sc in record.subcons if sc.name != "priv")
    has_priv = any(sc.name == "priv" for sc in record.subcons)
    return record.parse(bytes(fixed + (4 if has_priv else 0)))


def vendor_array(ttype: TupleType, elems) -> bytes:
    "snd_soc_tplg_vendor_array from [(token, value)]"
    body = b""
    for token, value in elems:
        if ttype == TupleType.UUID:
            body += struct.pack("<I", token) + bytes(value)
        elif ttype == TupleType.STRING:
            body += struct.pack("<I", token) + name(value)
        else:
            body += struct.pack("<II", token, value)
    return struct.pack("<III", 12 + len(body), ttype, len(elems)) + body


def tuples(*arrays) -> bytes:
    "Private data from (type, [(token, value)]) arrays."
    return b"".join(vendor_array(t, e) for t, e in arrays)


def words(**kw) -> tuple:
    "A WORD array from token=value keyword arguments, token names as in tokens below."
    return (TupleType.WORD, [(TOKENS[k], v) for k, v in kw.items()])


TOKENS = {
    "num_input_pins": 411, "num_output_pins": 412,
    "num_input_audio_formats": 415, "num_output_audio_formats": 416,
    "core": 404, "is_pages": 409, "node_type": 1980, "dai_index": 155,
    "lp_mode": 207, "chain_dma": 209, "sched_core": 203, "priority": 201, "dynamic": 206,
    "in_rate": 1900, "in_bit_depth": 1901, "in_ch_map": 1904, "in_ch_cfg": 1905,
    "in_interleaving": 1906, "in_fmt_cfg": 1907, "in_pin": 1909, "ibs": 1970,
    "out_rate": 1930, "out_bit_depth": 1931, "out_ch_map": 1934, "out_ch_cfg": 1935,
    "out_interleaving": 1936, "out_fmt_cfg": 1937, "out_pin": 1939, "obs": 1971,
}

COPIER_UUID = bytes.fromhex("830ca09b12ca834a943c1fa2e82f9dda")


def fmt_cfg(channels=2, valid=32, stype=1):
    return channels | valid << 8 | stype << 16


def in_format(rate=48000, depth=32, valid=32, channels=2, **extra):
    return words(in_rate=rate, in_bit_depth=depth, in_fmt_cfg=fmt_cfg(channels, valid),
                 in_ch_map=0xffffff10, in_ch_cfg=1, in_interleaving=0, ibs=384, **extra)


def out_format(rate=48000, depth=32, valid=32, channels=2, **extra):
    return words(out_rate=rate, out_bit_depth=depth, out_fmt_cfg=fmt_cfg(channels, valid),
                 out_ch_map=0xffffff10, out_ch_cfg=1, out_interleaving=0, obs=384, **extra)


def module_tuples(n_in=1, n_out=1, in_pins=1, out_pins=1, ins=None, outs=None, extra=()):
    "Typical module private data: pins, format counts, UUID and formats."
    ins = [in_format() for _ in range(n_in)] if ins is None else ins
    outs = [out_format() for _ in range(n_out)] if outs is None else outs
    return tuples(words(is_pages=1, num_input_audio_formats=n_in, num_output_audio_formats=n_out,
                        num_input_pins=in_pins, num_output_pins=out_pins),
                  (TupleType.UUID, [(405, COPIER_UUID)]), *extra, *ins, *outs)


class Topology:
    "Collects records and emits a topology binary."

    def __init__(self):
        self.widgets = []  # (index, widget container, [kcontrol containers])
        self.routes = []  # (index, container)
        self.pcms = []
        self.links = []
        self.manifest_priv = struct.pack("<HHH", 3, 30, 0)

    def widget(self, wname, wtype, index, priv=b"", sname="", kcontrols=()):
        w = zero(binary.WIDGET)
        w.size = SIZE["widget"]
        w.id = binary.DapmType[wtype.upper()]
        w.name, w.sname = name(wname), name(sname)
        w.priv = priv
        w.num_kcontrols = len(kcontrols)
        self.widgets.append((index, w, list(kcontrols)))
        return w

    def pipeline(self, index, chain_dma=0, dynamic=0, core=0):
        return self.widget(f"pipeline.{index}", "scheduler", index,
                           tuples(words(priority=0, lp_mode=0, sched_core=core),
                                  (TupleType.BOOL, [(209, chain_dma), (206, dynamic)])))

    def route(self, source, sink, index=0, control=""):
        r = zero(binary.GRAPH_ELEM)
        r.source, r.sink, r.control = name(source), name(sink), name(control)
        self.routes.append((index, r))

    def pcm(self, pname, pcm_id=0, playback=None, capture=None, formats=1 << 10,
            rates=1 << 7, channels=(2, 2)):
        p = zero(binary.PCM)
        p.size = SIZE["pcm"]
        p.pcm_name, p.dai_name, p.pcm_id = name(pname), name(pname), pcm_id
        for d, stream in enumerate((playback, capture)):
            if stream is None:
                continue
            setattr(p, ("playback", "capture")[d], 1)
            caps = p.caps[d]
            caps.size = SIZE["stream_caps"]
            caps.name = name(stream)
            caps.formats, caps.rates = formats, rates
            caps.rate_min, caps.rate_max = 8000, 192000
            caps.channels_min, caps.channels_max = channels
        self.pcms.append(p)
        return p

    def link(self, lname, link_id=0, dai_type="SSP", hw_ids=(0,), default_id=0):
        cfg = zero(binary.LINK_CONFIG)
        cfg.size = SIZE["link"]
        cfg.id, cfg.name, cfg.stream_name = link_id, name(lname), name(lname)
        for i, hw_id in enumerate(hw_ids):
            cfg.hw_config[i].size = SIZE["hw_config"]
            cfg.hw_config[i].id = hw_id
        cfg.num_hw_configs, cfg.default_hw_config_id = len(hw_ids), default_id
        cfg.priv = tuples((TupleType.STRING, [(154, dai_type)])) if dai_type else b""
        self.links.append(cfg)
        return cfg

    @staticmethod
    def kcontrol(kname, kind="MIXER", info=1, get=256, put=256, access=0x3, priv=b"",
                 tlv_type=1, **body):
        k = zero(binary.CTL_HDR)
        k.size = SIZE["ctl_hdr"]
        k.type = TplgType[kind]
        k.name = name(kname)
        k.access = access
        k.ops.info, k.ops.get, k.ops.put = info, get, put
        k.tlv.size, k.tlv.type = binary.CTL_TLV.sizeof(), tlv_type
        record = {"MIXER": binary.MIXER_BODY, "ENUM": binary.ENUM_BODY,
                  "BYTES": binary.BYTES_BODY}[kind]
        b = zero(record)
        b.size = SIZE[kind.lower()] - SIZE["ctl_hdr"]
        for key, value in body.items():
            b[key] = value
        b.priv = priv
        return {"hdr": k, "body": b}

    def _section(self, stype, records, index=0, count=None, extra=b""):
        payload = b"".join(records) + extra
        hdr = binary.HDR.build(dict(magic=binary.TPLG_MAGIC, abi=5, version=0, type=stype,
                                    size=binary.HDR_SIZE, vendor_type=0,
                                    payload_size=len(payload), index=index,
                                    count=len(records) if count is None else count))
        return hdr + payload

    def build(self) -> bytes:
        out = b""
        m = zero(binary.MANIFEST)
        m.size = SIZE["manifest"]
        m.control_elems = sum(len(k) for _i, _w, k in self.widgets)
        m.widget_elems, m.graph_elems = len(self.widgets), len(self.routes)
        m.pcm_elems, m.dai_link_elems = len(self.pcms), len(self.links)
        m.priv = self.manifest_priv
        out += self._section(TplgType.MANIFEST, [binary.MANIFEST.build(m)])
        for index, w, kcontrols in self.widgets:
            data = binary.WIDGET.build(w) + b"".join(binary.KCONTROL.build(k) for k in kcontrols)
            out += self._section(TplgType.DAPM_WIDGET, [data], index=index)
        if self.pcms:
            out += self._section(TplgType.PCM, [binary.PCM.build(p) for p in self.pcms])
        for index, r in self.routes:
            out += self._section(TplgType.DAPM_GRAPH, [binary.GRAPH_ELEM.build(r)], index=index)
        if self.links:
            out += self._section(TplgType.BACKEND_LINK,
                                 [binary.LINK_CONFIG.build(link) for link in self.links])
        return out


def playback_ssp(chain_dma=0) -> Topology:
    """A valid minimal topology: host copier -> gain -> SSP DAI copier, one
    PCM, one BE link."""
    t = Topology()
    t.pipeline(1, chain_dma=chain_dma)
    t.widget("host-copier.0.playback", "aif_in", 1, module_tuples(in_pins=0),
             sname="Port0 Playback")
    t.widget("gain.1.1", "pga", 1, module_tuples(),
             kcontrols=[Topology.kcontrol("Playback Volume", access=0x13, max=45,
                                          num_channels=2)])
    t.widget("dai-copier.SSP.NoCodec-0.playback", "dai_in", 1,
             module_tuples(out_pins=0, extra=[(TupleType.STRING, [(154, "SSP")]),
                                              words(dai_index=0, node_type=12)]),
             sname="NoCodec-0")
    t.route("host-copier.0.playback", "gain.1.1", 1)
    t.route("gain.1.1", "dai-copier.SSP.NoCodec-0.playback", 1)
    t.pcm("Port0", playback="Port0 Playback")
    t.link("NoCodec-0")
    return t
