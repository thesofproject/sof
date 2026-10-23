# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2026 Intel Corporation. All rights reserved.

"""ALSA topology binary format, ABI 5 (Linux include/uapi/sound/asoc.h).

The record layouts are ported from sof-test tools/tplgtool2.py. Unlike
tplgtool2 the section loop is driven by hand, mirroring soc_tplg_load()
in Linux sound/soc/soc-topology.c, so that malformed input is reported
as an issue at a file offset instead of an exception from construct.
All values are kept as plain integers and raw bytes; interpretation is
left to the model.
"""

import enum
import io
from dataclasses import dataclass, field

import construct
from construct import (Array, Bytes, GreedyBytes, Int8ul, Int16ul, Int32sl,
                       Int32ul, Int64ul, Prefixed, Struct, Switch, this)

TPLG_MAGIC = 0x41536F43  # SND_SOC_TPLG_MAGIC, "CoSA" in file byte order
TPLG_MAGIC_BE = 0x436F5341
TPLG_ABI_VERSION = 5  # SND_SOC_TPLG_ABI_VERSION
TPLG_ABI_VERSION_MIN = 5  # SND_SOC_TPLG_ABI_VERSION_MIN

NAME_LEN = 44  # SNDRV_CTL_ELEM_ID_NAME_MAXLEN
MAX_CHAN = 8  # SND_SOC_TPLG_MAX_CHAN
STREAM_CONFIG_MAX = 8  # SND_SOC_TPLG_STREAM_CONFIG_MAX
HW_CONFIG_MAX = 8  # SND_SOC_TPLG_HW_CONFIG_MAX
TLV_SIZE = 32  # SND_SOC_TPLG_TLV_SIZE
NUM_TEXTS = 16  # SND_SOC_TPLG_NUM_TEXTS


class TplgType(enum.IntEnum):
    "SND_SOC_TPLG_TYPE_*"
    MIXER = 1
    BYTES = 2
    ENUM = 3
    DAPM_GRAPH = 4
    DAPM_WIDGET = 5
    DAI_LINK = 6
    PCM = 7
    MANIFEST = 8
    CODEC_LINK = 9
    BACKEND_LINK = 10
    PDATA = 11
    DAI = 12


class TupleType(enum.IntEnum):
    "SND_SOC_TPLG_TUPLE_TYPE_*"
    UUID = 0
    STRING = 1
    BOOL = 2
    BYTE = 3
    WORD = 4
    SHORT = 5


class DapmType(enum.IntEnum):
    "SND_SOC_TPLG_DAPM_*"
    INPUT = 0
    OUTPUT = 1
    MUX = 2
    MIXER = 3
    PGA = 4
    OUT_DRV = 5
    ADC = 6
    DAC = 7
    SWITCH = 8
    PRE = 9
    POST = 10
    AIF_IN = 11
    AIF_OUT = 12
    DAI_IN = 13
    DAI_OUT = 14
    DAI_LINK = 15
    BUFFER = 16
    SCHEDULER = 17
    EFFECT = 18
    SIGGEN = 19
    SRC = 20
    ASRC = 21
    ENCODER = 22
    DECODER = 23


Name = Bytes(NAME_LEN)

HDR = Struct(  # snd_soc_tplg_hdr
    "magic" / Int32ul,
    "abi" / Int32ul,
    "version" / Int32ul,
    "type" / Int32ul,
    "size" / Int32ul,
    "vendor_type" / Int32ul,
    "payload_size" / Int32ul,
    "index" / Int32ul,
    "count" / Int32ul,
)
HDR_SIZE = HDR.sizeof()

PRIVATE = Prefixed(Int32ul, GreedyBytes)  # snd_soc_tplg_private

IO_OPS = Struct(  # snd_soc_tplg_io_ops
    "get" / Int32ul,
    "put" / Int32ul,
    "info" / Int32ul,
)

CTL_TLV = Struct(  # snd_soc_tplg_ctl_tlv
    "size" / Int32ul,
    "type" / Int32ul,
    "data" / Array(TLV_SIZE, Int32sl),  # union with snd_soc_tplg_tlv_dbscale
)

CTL_HDR = Struct(  # snd_soc_tplg_ctl_hdr
    "size" / Int32ul,
    "type" / Int32ul,
    "name" / Name,
    "access" / Int32ul,
    "ops" / IO_OPS,
    "tlv" / CTL_TLV,
)

CHANNEL = Struct(  # snd_soc_tplg_channel
    "size" / Int32ul,
    "reg" / Int32ul,
    "shift" / Int32ul,
    "id" / Int32ul,
)

MIXER_BODY = Struct(  # snd_soc_tplg_mixer_control without hdr
    "size" / Int32ul,
    "min" / Int32sl,
    "max" / Int32sl,
    "platform_max" / Int32sl,
    "invert" / Int32ul,
    "num_channels" / Int32ul,
    "channel" / Array(MAX_CHAN, CHANNEL),
    "priv" / PRIVATE,
)

ENUM_BODY = Struct(  # snd_soc_tplg_enum_control without hdr
    "size" / Int32ul,
    "num_channels" / Int32ul,
    "channel" / Array(MAX_CHAN, CHANNEL),
    "items" / Int32ul,
    "mask" / Int32ul,
    "count" / Int32ul,
    "texts" / Array(NUM_TEXTS, Name),
    "values" / Array(NUM_TEXTS * NAME_LEN // 4, Int32ul),
    "priv" / PRIVATE,
)

BYTES_BODY = Struct(  # snd_soc_tplg_bytes_control without hdr
    "size" / Int32ul,
    "max" / Int32ul,
    "mask" / Int32ul,
    "base" / Int32ul,
    "num_regs" / Int32ul,
    "ext_ops" / IO_OPS,
    "priv" / PRIVATE,
)

KCONTROL = Struct(
    "hdr" / CTL_HDR,
    "body" / Switch(this.hdr.type, {
        TplgType.MIXER: MIXER_BODY,
        TplgType.ENUM: ENUM_BODY,
        TplgType.BYTES: BYTES_BODY,
    }, default=construct.Error),
)

STREAM_CAPS = Struct(  # snd_soc_tplg_stream_caps
    "size" / Int32ul,
    "name" / Name,
    "formats" / Int64ul,
    "rates" / Int32ul,
    "rate_min" / Int32ul,
    "rate_max" / Int32ul,
    "channels_min" / Int32ul,
    "channels_max" / Int32ul,
    "periods_min" / Int32ul,
    "periods_max" / Int32ul,
    "period_size_min" / Int32ul,
    "period_size_max" / Int32ul,
    "buffer_size_min" / Int32ul,
    "buffer_size_max" / Int32ul,
    "sig_bits" / Int32ul,
)

STREAM = Struct(  # snd_soc_tplg_stream
    "size" / Int32ul,
    "name" / Name,
    "format" / Int64ul,
    "rate" / Int32ul,
    "period_bytes" / Int32ul,
    "buffer_bytes" / Int32ul,
    "channels" / Int32ul,
)

HW_CONFIG = Struct(  # snd_soc_tplg_hw_config
    "size" / Int32ul,
    "id" / Int32ul,
    "fmt" / Int32ul,
    "clock_gated" / Int8ul,
    "invert_bclk" / Int8ul,
    "invert_fsync" / Int8ul,
    "bclk_provider" / Int8ul,
    "fsync_provider" / Int8ul,
    "mclk_direction" / Int8ul,
    "reserved" / Int16ul,
    "mclk_rate" / Int32ul,
    "bclk_rate" / Int32ul,
    "fsync_rate" / Int32ul,
    "tdm_slots" / Int32ul,
    "tdm_slot_width" / Int32ul,
    "tx_slots" / Int32ul,
    "rx_slots" / Int32ul,
    "tx_channels" / Int32ul,
    "tx_chanmap" / Array(MAX_CHAN, Int32ul),
    "rx_channels" / Int32ul,
    "rx_chanmap" / Array(MAX_CHAN, Int32ul),
)

MANIFEST = Struct(  # snd_soc_tplg_manifest
    "size" / Int32ul,
    "control_elems" / Int32ul,
    "widget_elems" / Int32ul,
    "graph_elems" / Int32ul,
    "pcm_elems" / Int32ul,
    "dai_link_elems" / Int32ul,
    "dai_elems" / Int32ul,
    "reserved" / Array(20, Int32ul),
    "priv" / PRIVATE,
)

GRAPH_ELEM = Struct(  # snd_soc_tplg_dapm_graph_elem
    "sink" / Name,
    "control" / Name,
    "source" / Name,
)

WIDGET = Struct(  # snd_soc_tplg_dapm_widget
    "size" / Int32ul,
    "id" / Int32ul,
    "name" / Name,
    "sname" / Name,
    "reg" / Int32sl,
    "shift" / Int32ul,
    "mask" / Int32ul,
    "subseq" / Int32ul,
    "invert" / Int32ul,
    "ignore_suspend" / Int32ul,
    "event_flags" / Int16ul,
    "event_type" / Int16ul,
    "num_kcontrols" / Int32ul,
    "priv" / PRIVATE,
)

PCM = Struct(  # snd_soc_tplg_pcm
    "size" / Int32ul,
    "pcm_name" / Name,
    "dai_name" / Name,
    "pcm_id" / Int32ul,
    "dai_id" / Int32ul,
    "playback" / Int32ul,
    "capture" / Int32ul,
    "compress" / Int32ul,
    "stream" / Array(STREAM_CONFIG_MAX, STREAM),
    "num_streams" / Int32ul,
    "caps" / Array(2, STREAM_CAPS),
    "flag_mask" / Int32ul,
    "flags" / Int32ul,
    "priv" / PRIVATE,
)

LINK_CONFIG = Struct(  # snd_soc_tplg_link_config
    "size" / Int32ul,
    "id" / Int32ul,
    "name" / Name,
    "stream_name" / Name,
    "stream" / Array(STREAM_CONFIG_MAX, STREAM),
    "num_streams" / Int32ul,
    "hw_config" / Array(HW_CONFIG_MAX, HW_CONFIG),
    "num_hw_configs" / Int32ul,
    "default_hw_config_id" / Int32ul,
    "flag_mask" / Int32ul,
    "flags" / Int32ul,
    "priv" / PRIVATE,
)

DAI = Struct(  # snd_soc_tplg_dai
    "size" / Int32ul,
    "dai_name" / Name,
    "dai_id" / Int32ul,
    "playback" / Int32ul,
    "capture" / Int32ul,
    "caps" / Array(2, STREAM_CAPS),
    "flag_mask" / Int32ul,
    "flags" / Int32ul,
    "priv" / PRIVATE,
)


def struct_size(record: Struct) -> int:
    """sizeof() of a kernel struct ending in snd_soc_tplg_private.

    The flexible private data is not part of the C struct, but its
    32-bit size field is.
    """
    fixed = Struct(*[sc for sc in record.subcons if sc.name != "priv"])
    return fixed.sizeof() + (4 if "priv" in record._subcons else 0)


SIZEOF = {
    "hdr": HDR_SIZE,
    "ctl_hdr": CTL_HDR.sizeof(),
    "mixer": CTL_HDR.sizeof() + struct_size(MIXER_BODY),
    "enum": CTL_HDR.sizeof() + struct_size(ENUM_BODY),
    "bytes": CTL_HDR.sizeof() + struct_size(BYTES_BODY),
    "manifest": struct_size(MANIFEST),
    "widget": struct_size(WIDGET),
    "pcm": struct_size(PCM),
    "link": struct_size(LINK_CONFIG),
    "dai": struct_size(DAI),
    "stream_caps": STREAM_CAPS.sizeof(),
    "stream": STREAM.sizeof(),
    "hw_config": HW_CONFIG.sizeof(),
    "channel": CHANNEL.sizeof(),
}


@dataclass
class Issue:
    "A structural problem found while walking the binary."
    code: str
    offset: int
    message: str


@dataclass
class Element:
    "One parsed element of a section, with its absolute file offset."
    offset: int
    data: construct.Container
    kcontrols: list = field(default_factory=list)  # widgets only: [Element]


@dataclass
class Section:
    "One snd_soc_tplg_hdr block and its parsed elements."
    offset: int
    hdr: construct.Container
    elements: list = field(default_factory=list)  # [Element]

    @property
    def type_name(self) -> str:
        try:
            return TplgType(self.hdr.type).name
        except ValueError:
            return f"type {self.hdr.type}"


@dataclass
class RawTopology:
    "Result of walking a topology binary."
    size: int
    sections: list = field(default_factory=list)  # [Section]
    issues: list = field(default_factory=list)  # [Issue]


ELEMENT_RECORD = {
    TplgType.MIXER: KCONTROL,
    TplgType.ENUM: KCONTROL,
    TplgType.BYTES: KCONTROL,
    TplgType.DAPM_GRAPH: GRAPH_ELEM,
    TplgType.DAPM_WIDGET: WIDGET,
    TplgType.PCM: PCM,
    TplgType.DAI_LINK: LINK_CONFIG,
    TplgType.BACKEND_LINK: LINK_CONFIG,
    TplgType.MANIFEST: MANIFEST,
    TplgType.DAI: DAI,
}


def _parse_elements(raw: RawTopology, sec: Section, payload: bytes):
    "Parse hdr.count elements of a known section type from its payload."
    base = sec.offset + HDR_SIZE
    record = ELEMENT_RECORD[sec.hdr.type]
    count = 1 if sec.hdr.type == TplgType.MANIFEST else sec.hdr.count
    stream = io.BytesIO(payload)

    for i in range(count):
        pos = stream.tell()
        try:
            elem = Element(base + pos, record.parse_stream(stream))
            if sec.hdr.type == TplgType.DAPM_WIDGET:
                for _ in range(elem.data.num_kcontrols):
                    kpos = stream.tell()
                    elem.kcontrols.append(Element(base + kpos, KCONTROL.parse_stream(stream)))
        except construct.ConstructError as e:
            raw.issues.append(Issue(
                "elem-truncated", base + pos,
                f"{sec.type_name} element {i}/{count} does not fit in the "
                f"section payload or is malformed: {str(e).splitlines()[0]}"))
            return
        sec.elements.append(elem)

    left = len(payload) - stream.tell()
    if left:
        raw.issues.append(Issue(
            "payload-trailing", base + stream.tell(),
            f"{sec.type_name} section has {left} unparsed bytes after {count} elements"))


def parse_bytes(data: bytes) -> RawTopology:
    "Walk all section headers like soc_tplg_process_headers() does."
    raw = RawTopology(len(data))
    pos = 0

    while pos < len(data):
        if len(data) - pos < HDR_SIZE:
            raw.issues.append(Issue("hdr-truncated", pos,
                                    f"{len(data) - pos} trailing bytes, too short for a header"))
            break

        hdr = HDR.parse(data[pos:pos + HDR_SIZE])
        sec = Section(pos, hdr)

        # soc_tplg_valid_header()
        fatal = False
        if hdr.magic == TPLG_MAGIC_BE:
            raw.issues.append(Issue("hdr-magic", pos, "big-endian topology, not supported"))
            fatal = True
        elif hdr.magic != TPLG_MAGIC:
            raw.issues.append(Issue("hdr-magic", pos, f"bad magic 0x{hdr.magic:08x}"))
            fatal = True
        if hdr.size != HDR_SIZE:
            raw.issues.append(Issue("hdr-size", pos,
                                    f"header size {hdr.size}, expected {HDR_SIZE}"))
            fatal = True
        if fatal:
            break

        if not TPLG_ABI_VERSION_MIN <= hdr.abi <= TPLG_ABI_VERSION:
            raw.issues.append(Issue("hdr-abi", pos,
                                    f"ABI version {hdr.abi}, kernel supports {TPLG_ABI_VERSION}"))
        if hdr.payload_size == 0:
            raw.issues.append(Issue("hdr-payload", pos,
                                    f"{sec.type_name} section has empty payload"))

        end = pos + HDR_SIZE + hdr.payload_size
        if end > len(data):
            raw.issues.append(Issue("hdr-payload", pos,
                                    f"{sec.type_name} payload of {hdr.payload_size} bytes "
                                    f"extends past end of file"))
            break

        raw.sections.append(sec)
        if hdr.type in ELEMENT_RECORD.keys():
            _parse_elements(raw, sec, data[pos + HDR_SIZE:end])
        pos = end

    return raw


def parse_file(path) -> RawTopology:
    with open(path, "rb") as f:
        return parse_bytes(f.read())


def decode_name(raw: bytes) -> str:
    "Decode a fixed size, NUL padded name field."
    return raw.split(b"\0", 1)[0].decode("ascii", errors="replace")


def name_terminated(raw: bytes) -> bool:
    "Mirror strnlen(name, SNDRV_CTL_ELEM_ID_NAME_MAXLEN) < SNDRV_CTL_ELEM_ID_NAME_MAXLEN."
    return b"\0" in raw
