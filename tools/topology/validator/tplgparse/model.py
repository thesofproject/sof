# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2026 Intel Corporation. All rights reserved.

"""Typed view of a parsed topology binary.

Vendor tuples are decoded here by the type stored in each
snd_soc_tplg_vendor_array, i.e. the way the topology author meant them.
How the kernel actually reads them is emulated in kernel_engine.py.
"""

import struct
from dataclasses import dataclass, field
from typing import Optional

from . import binary
from .binary import DapmType, TplgType, TupleType, decode_name

ARRAY_HDR_SIZE = 12  # sizeof(struct snd_soc_tplg_vendor_array)
ELEM_SIZE = {
    TupleType.UUID: 4 + 16,  # snd_soc_tplg_vendor_uuid_elem
    TupleType.STRING: 4 + binary.NAME_LEN,  # snd_soc_tplg_vendor_string_elem
    TupleType.BOOL: 8,  # snd_soc_tplg_vendor_value_elem
    TupleType.BYTE: 8,
    TupleType.WORD: 8,
    TupleType.SHORT: 8,
}


@dataclass
class VendorTuple:
    token: int
    type: TupleType
    value: object  # int, str or 16 bytes for UUID
    array: int  # index of the vendor array in the private data
    offset: int  # offset of the element within the private data


@dataclass
class VendorArray:
    offset: int  # within the private data
    size: int
    type: int
    num_elems: int
    tuples: list = field(default_factory=list)


@dataclass
class PrivIssue:
    offset: int  # within the private data
    message: str


@dataclass
class Private:
    "snd_soc_tplg_private data interpreted as consecutive vendor arrays."
    data: bytes
    arrays: list = field(default_factory=list)
    issues: list = field(default_factory=list)

    @property
    def tuples(self):
        return [t for a in self.arrays for t in a.tuples]

    def values(self, token: int) -> list:
        return [t.value for t in self.tuples if t.token == token]

    def value(self, token: int, default=None):
        vals = self.values(token)
        return vals[0] if vals else default


def decode_private(data: bytes) -> Private:
    "Walk vendor arrays by their declared size and type."
    priv = Private(data)
    pos = 0
    while pos < len(data):
        if len(data) - pos < ARRAY_HDR_SIZE:
            priv.issues.append(PrivIssue(pos, f"{len(data) - pos} trailing bytes are too short "
                                              f"for a vendor array header"))
            break
        size, atype, num = struct.unpack_from("<III", data, pos)
        arr = VendorArray(pos, size, atype, num)
        if size < ARRAY_HDR_SIZE or pos + size > len(data):
            priv.issues.append(PrivIssue(pos, f"vendor array size {size} is invalid "
                                              f"({len(data) - pos} bytes left)"))
            break
        try:
            ttype = TupleType(atype)
        except ValueError:
            priv.issues.append(PrivIssue(pos, f"unknown vendor array type {atype}"))
            pos += size
            continue
        esize = ELEM_SIZE[ttype]
        if ARRAY_HDR_SIZE + num * esize != size:
            priv.issues.append(PrivIssue(
                pos, f"{ttype.name} vendor array size {size} does not match "
                     f"{num} elements of {esize} bytes"))
            num = min(num, (size - ARRAY_HDR_SIZE) // esize)
        for i in range(num):
            epos = pos + ARRAY_HDR_SIZE + i * esize
            token = struct.unpack_from("<I", data, epos)[0]
            if ttype == TupleType.UUID:
                value = bytes(data[epos + 4:epos + 20])
            elif ttype == TupleType.STRING:
                raw = data[epos + 4:epos + 4 + binary.NAME_LEN]
                if not binary.name_terminated(raw):
                    priv.issues.append(PrivIssue(epos, f"string tuple {token} is not "
                                                       f"NUL terminated"))
                value = decode_name(raw)
            else:
                value = struct.unpack_from("<I", data, epos + 4)[0]
            arr.tuples.append(VendorTuple(token, ttype, value, len(priv.arrays), epos))
        priv.arrays.append(arr)
        pos += size
    return priv


def format_uuid(raw: bytes) -> str:
    "Format like the kernel's %pUL."
    a, b, c = struct.unpack_from("<IHH", raw)
    return f"{a:08x}-{b:04x}-{c:04x}-{raw[8:10].hex()}-{raw[10:16].hex()}"


@dataclass
class Control:
    kind: TplgType  # MIXER, ENUM or BYTES
    name: str
    hdr: object
    body: object
    priv: Private
    offset: int
    widget: Optional["Widget"] = None

    @property
    def label(self) -> str:
        return f"kcontrol '{self.name}'"


@dataclass
class Widget:
    index: int  # section header index, the kernel's swidget->pipeline_id
    id: int  # SND_SOC_TPLG_DAPM_*
    name: str
    sname: str
    raw: object
    priv: Private
    offset: int
    kcontrols: list = field(default_factory=list)  # [Control]

    @property
    def type_name(self) -> str:
        "Kernel snd_soc_dapm_* name, e.g. 'aif_in'."
        try:
            return DapmType(self.id).name.lower()
        except ValueError:
            return f"type{self.id}"

    @property
    def label(self) -> str:
        return f"widget '{self.name}'"


@dataclass
class Route:
    index: int
    sink: str
    control: str
    source: str
    raw: object
    offset: int

    @property
    def label(self) -> str:
        ctl = f" via '{self.control}'" if self.control else ""
        return f"route '{self.source}' -> '{self.sink}'{ctl}"


@dataclass
class Pcm:
    name: str
    dai_name: str
    raw: object
    priv: Private
    offset: int

    @property
    def label(self) -> str:
        return f"PCM '{self.name}' (id {self.raw.pcm_id})"


@dataclass
class Link:
    name: str
    stream_name: str
    raw: object
    priv: Private
    offset: int
    section_type: TplgType = TplgType.BACKEND_LINK

    @property
    def hw_configs(self) -> list:
        return list(self.raw.hw_config[:self.raw.num_hw_configs])

    @property
    def label(self) -> str:
        return f"link '{self.name}' (id {self.raw.id})"


@dataclass
class Dai:
    name: str
    raw: object
    priv: Private
    offset: int

    @property
    def label(self) -> str:
        return f"DAI '{self.name}'"


@dataclass
class Topology:
    path: str
    raw: binary.RawTopology
    manifest: Optional[object] = None
    widgets: list = field(default_factory=list)
    routes: list = field(default_factory=list)
    controls: list = field(default_factory=list)  # standalone and widget kcontrols
    pcms: list = field(default_factory=list)
    links: list = field(default_factory=list)
    dais: list = field(default_factory=list)
    # widgets and routes of topologies loaded before this one into the
    # same component, e.g. function topologies under a feature topology
    base_widgets: list = field(default_factory=list)
    base_routes: list = field(default_factory=list)

    def widget(self, name: str) -> Optional[Widget]:
        "Look up a widget by name, including widgets of base topologies."
        return self._by_name.get(name)

    def __post_init__(self):
        self._by_name = {}

    def index(self):
        self._by_name = {}
        for w in self.widgets + self.base_widgets:
            self._by_name.setdefault(w.name, w)

    def add_base(self, base: "Topology"):
        "Make the objects of a topology loaded earlier visible to this one."
        self.base_widgets += base.widgets + base.base_widgets
        self.base_routes += base.routes + base.base_routes
        self.index()


def _control(elem: binary.Element, widget=None) -> Control:
    hdr, body = elem.data.hdr, elem.data.body
    return Control(TplgType(hdr.type), decode_name(hdr.name), hdr, body,
                   decode_private(body.priv), elem.offset, widget)


def build(raw: binary.RawTopology, path: str = "") -> Topology:
    tplg = Topology(path, raw)
    for sec in raw.sections:
        stype = sec.hdr.type
        for elem in sec.elements:
            d = elem.data
            if stype in (TplgType.MIXER, TplgType.ENUM, TplgType.BYTES):
                tplg.controls.append(_control(elem))
            elif stype == TplgType.DAPM_WIDGET:
                w = Widget(sec.hdr.index, d.id, decode_name(d.name), decode_name(d.sname), d,
                           decode_private(d.priv), elem.offset)
                w.kcontrols = [_control(k, w) for k in elem.kcontrols]
                tplg.controls.extend(w.kcontrols)
                tplg.widgets.append(w)
            elif stype == TplgType.DAPM_GRAPH:
                tplg.routes.append(Route(sec.hdr.index, decode_name(d.sink),
                                         decode_name(d.control), decode_name(d.source), d,
                                         elem.offset))
            elif stype == TplgType.PCM:
                tplg.pcms.append(Pcm(decode_name(d.pcm_name), decode_name(d.dai_name), d,
                                     decode_private(d.priv), elem.offset))
            elif stype in (TplgType.DAI_LINK, TplgType.BACKEND_LINK):
                tplg.links.append(Link(decode_name(d.name), decode_name(d.stream_name), d,
                                       decode_private(d.priv), elem.offset, TplgType(stype)))
            elif stype == TplgType.DAI:
                tplg.dais.append(Dai(decode_name(d.dai_name), d, decode_private(d.priv),
                                     elem.offset))
            elif stype == TplgType.MANIFEST:
                tplg.manifest = elem
    tplg.index()
    return tplg


def load(path) -> Topology:
    return build(binary.parse_file(path), str(path))
