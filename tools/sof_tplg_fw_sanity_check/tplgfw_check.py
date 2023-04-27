#!/usr/bin/env python3

import argparse
import construct
import enum
import os
import re
import sys
import typing
from collections import defaultdict
from construct import this, Container, ListContainer, Struct, Switch, Array, Bytes, GreedyBytes, GreedyRange, FocusedSeq, Pass, Padded, Padding, Prefixed, Flag, Byte, Int16ul, Int32ul, Int64ul, Terminated
from dataclasses import dataclass
from functools import cached_property, partial

# Pylint complain about the missing names even in conditional import.
# pylint: disable=E0611
if construct.version < (2, 9, 0):
    from construct import Const, String

    def __enum_dict(enumtype):
        return dict((e.name, e.value) for e in enumtype)
    # here is a workaround to make Enum/FlagsEnum accept enum type, like construct 2.9
    def Enum(subcon, enumtype):
        return construct.Enum(subcon, default=Pass, **__enum_dict(enumtype))
    def FlagsEnum(subcon, enumtype):
        return construct.FlagsEnum(subcon, **__enum_dict(enumtype))

if construct.version >= (2, 9, 0):
    # https://github.com/construct/construct/blob/master/docs/transition29.rst
    # Pylint complain about the argument reodering
    # pylint: disable=E0102,W1114
    from construct import PaddedString as String
    from construct import Enum, FlagsEnum

    def Const(subcon, value):
        return construct.Const(value, subcon)

def CompleteRange(subcon):
    "like GreedyRange, but will report error if anything left in stream."
    return FocusedSeq("range",
            "range" / GreedyRange(subcon),
            Terminated
        )

def get_flags(flagsenum: Container) -> "list[str]":
    "Get flags for FlagsEnum container."
    return [name for (name, value) in flagsenum.items() if value is True and not name.startswith("_")]

class TplgType(enum.IntEnum):
    r"""File and Block header data types.

    `SND_SOC_TPLG_TYPE_`
    """
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

class VendorTupleType(enum.IntEnum):
    r"""vendor tuple types.

    `SND_SOC_TPLG_TUPLE_TYPE_`
    """
    UUID = 0
    STRING = 1
    BOOL = 2
    BYTE = 3
    WORD = 4
    SHORT = 5

class DaiFormat(enum.IntEnum):
    r"""DAI physical PCM data formats.

    `SND_SOC_DAI_FORMAT_`
    """
    I2S = 1
    RIGHT_J = 2
    LEFT_J = 3
    DSP_A = 4
    DSP_B = 5
    AC97 = 6
    PDM = 7

class DaiLnkFlag(enum.IntFlag):
    r"""DAI link flags.

    `SND_SOC_TPLG_LNK_FLGBIT_`
    """
    SYMMETRIC_RATES = 0b0001
    SYMMETRIC_CHANNELS = 0b0010
    SYMMETRIC_SAMPLEBITS = 0b0100
    VOICE_WAKEUP = 0b1000

class DaiMClk(enum.IntEnum):
    r"""DAI mclk_direction.

    `SND_SOC_TPLG_MCLK_`
    """
    CO = 0
    CI = 1

class DaiBClk(enum.IntEnum):
    r"""DAI topology BCLK parameter.

    `SND_SOC_TPLG_BCLK_`
    """
    CP = 0
    CC = 1

class DaiFSync(enum.IntEnum):
    r"""DAI topology FSYNC parameter.

    `SND_SOC_TPLG_FSYNC_`
    """
    CP = 0
    CC = 1

class DaiClockGate(enum.IntEnum):
    r"""DAI clock gating.

    `SND_SOC_TPLG_DAI_CLK_GATE_`
    """
    UNDEFINED = 0
    GATED = 1
    CONT = 2

class CtlTlvt(enum.IntEnum):
    "`SNDRV_CTL_TLVT_`"
    CONTAINER = 0
    DB_SCALE = 1
    DB_LINEAR = 2
    DB_RANGE = 3
    DB_MINMAX = 4
    DB_MINMAX_MUTE = 5

    CHMAP_FIXED = 0x101
    CHMAP_VAR = 0x102
    CHMAP_PAIRED = 0x103

class DapmType(enum.IntEnum):
    "`SND_SOC_TPLG_DAPM_`"
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

class PcmFormatsFlag(enum.IntFlag):
    r"""PCM sample formats.

    `SND_PCM_FMTBIT_LINEAR` and `SND_PCM_FMTBIT_FLOAT`
    """
    S8 = 1 << 0
    U8 = 1 << 1
    S16_LE = 1 << 2
    S16_BE = 1 << 3
    U16_LE = 1 << 4
    U16_BE = 1 << 5
    S24_LE = 1 << 6
    S24_BE = 1 << 7
    U24_LE = 1 << 8
    U24_BE = 1 << 9
    S32_LE = 1 << 10
    S32_BE = 1 << 11
    U32_LE = 1 << 12
    U32_BE = 1 << 13

    FLOAT_LE = 1 << 14
    FLOAT_BE = 1 << 15

class SofVendorToken(enum.IntEnum):
    r"""SOF vendor tokens

    See `tools/topology1/sof/tokens.m4` in SOF.
    """
    # sof_buffer_tokens
    SOF_TKN_BUF_SIZE = 100
    SOF_TKN_BUF_CAPS = 101
    # sof_dai_tokens
    SOF_TKN_DAI_TYPE = 154
    SOF_TKN_DAI_INDEX = 155
    SOF_TKN_DAI_DIRECTION = 156
    # sof_sched_tokens
    SOF_TKN_SCHED_PERIOD = 200
    SOF_TKN_SCHED_PRIORITY = 201
    SOF_TKN_SCHED_MIPS = 202
    SOF_TKN_SCHED_CORE = 203
    SOF_TKN_SCHED_FRAMES = 204
    SOF_TKN_SCHED_TIME_DOMAIN = 205
    SOF_TKN_SCHED_DYNAMIC_PIPELINE = 206
    # sof_volume_tokens
    SOF_TKN_VOLUME_RAMP_STEP_TYPE = 250
    SOF_TKN_VOLUME_RAMP_STEP_MS = 251
    # sof_src_tokens
    SOF_TKN_SRC_RATE_IN = 300
    SOF_TKN_SRC_RATE_OUT = 301
    # sof_asrc_tokens
    SOF_TKN_ASRC_RATE_IN = 320
    SOF_TKN_ASRC_RATE_OUT = 321
    SOF_TKN_ASRC_ASYNCHRONOUS_MODE = 322
    SOF_TKN_ASRC_OPERATION_MODE = 323
    # sof_pcm_tokens
    SOF_TKN_PCM_DMAC_CONFIG = 353
    # sof_comp_tokens
    SOF_TKN_COMP_PERIOD_SINK_COUNT = 400
    SOF_TKN_COMP_PERIOD_SOURCE_COUNT = 401
    SOF_TKN_COMP_FORMAT = 402
    SOF_TKN_COMP_CORE_ID = 404
    SOF_TKN_COMP_UUID = 405
    # sof_ssp_tokens
    SOF_TKN_INTEL_SSP_CLKS_CONTROL = 500
    SOF_TKN_INTEL_SSP_MCLK_ID = 501
    SOF_TKN_INTEL_SSP_SAMPLE_BITS = 502
    SOF_TKN_INTEL_SSP_FRAME_PULSE_WIDTH = 503
    SOF_TKN_INTEL_SSP_QUIRKS = 504
    SOF_TKN_INTEL_SSP_TDM_PADDING_PER_SLOT = 505
    SOF_TKN_INTEL_SSP_BCLK_DELAY = 506
    # sof_dmic_tokens
    SOF_TKN_INTEL_DMIC_DRIVER_VERSION = 600
    SOF_TKN_INTEL_DMIC_CLK_MIN = 601
    SOF_TKN_INTEL_DMIC_CLK_MAX = 602
    SOF_TKN_INTEL_DMIC_DUTY_MIN = 603
    SOF_TKN_INTEL_DMIC_DUTY_MAX = 604
    SOF_TKN_INTEL_DMIC_NUM_PDM_ACTIVE = 605
    SOF_TKN_INTEL_DMIC_SAMPLE_RATE = 608
    SOF_TKN_INTEL_DMIC_FIFO_WORD_LENGTH = 609
    SOF_TKN_INTEL_DMIC_UNMUTE_RAMP_TIME_MS = 610
    # sof_dmic_pdm_tokens
    SOF_TKN_INTEL_DMIC_PDM_CTRL_ID = 700
    SOF_TKN_INTEL_DMIC_PDM_MIC_A_Enable = 701
    SOF_TKN_INTEL_DMIC_PDM_MIC_B_Enable = 702
    SOF_TKN_INTEL_DMIC_PDM_POLARITY_A = 703
    SOF_TKN_INTEL_DMIC_PDM_POLARITY_B = 704
    SOF_TKN_INTEL_DMIC_PDM_CLK_EDGE = 705
    SOF_TKN_INTEL_DMIC_PDM_SKEW = 706
    # sof_tone_tokens
    SOF_TKN_TONE_SAMPLE_RATE = 800
    # sof_process_tokens
    SOF_TKN_PROCESS_TYPE = 900
    # sof_sai_tokens
    SOF_TKN_IMX_SAI_MCLK_ID = 1000
    # sof_esai_tokens
    SOF_TKN_IMX_ESAI_MCLK_ID = 1100
    # sof_stream_tokens
    SOF_TKN_STREAM_PLAYBACK_COMPATIBLE_D0I3 = 1200
    SOF_TKN_STREAM_CAPTURE_COMPATIBLE_D0I3 = 1201
    # sof_led_tokens
    SOF_TKN_MUTE_LED_USE = 1300
    SOF_TKN_MUTE_LED_DIRECTION = 1301
    # sof_alh_tokens
    SOF_TKN_INTEL_ALH_RATE = 1400
    SOF_TKN_INTEL_ALH_CH = 1401
    # sof_hda_tokens
    SOF_TKN_INTEL_HDA_RATE = 1500
    SOF_TKN_INTEL_HDA_CH = 1501
    # sof_afe_tokens
    SOF_TKN_MEDIATEK_AFE_RATE = 1600
    SOF_TKN_MEDIATEK_AFE_CH = 1601
    SOF_TKN_MEDIATEK_AFE_FORMAT = 1602

# Pylint complain about too many instance attributes, but they are necessary here.
# pylint: disable=R0902
class TplgBinaryFormat:
    r"""Topology binary format description.

    To parse and build topology binary data.
    """

    _TPLG_MAGIC = b'CoSA'
    _ABI_HEADER = Struct(
        "magic" / Const(Bytes(len(_TPLG_MAGIC)), _TPLG_MAGIC),
        "abi" / Int32ul,
    )

    @staticmethod
    def parse_abi_version(filepath) -> int:
        "Recognize the ABI version for TPLG file."
        with open(os.fspath(filepath), "rb") as fs:
            return TplgBinaryFormat._ABI_HEADER.parse_stream(fs)["abi"]

    def __init__(self):
        "Initialize the topology binary format."
        self._abi = 5 # SND_SOC_TPLG_ABI_VERSION
        self._max_channel = 8 # SND_SOC_TPLG_MAX_CHAN
        self._max_formats = 16 # SND_SOC_TPLG_MAX_FORMATS
        self._stream_config_max = 8 # SND_SOC_TPLG_STREAM_CONFIG_MAX
        self._hw_config_max = 8 # SND_SOC_TPLG_HW_CONFIG_MAX
        self._tlv_size = 32 # SND_SOC_TPLG_TLV_SIZE
        self._id_name_maxlen = 44 # SNDRV_CTL_ELEM_ID_NAME_MAXLEN
        self._num_texts = 16 # SND_SOC_TPLG_NUM_TEXTS

        self._block_header = Struct( # snd_soc_tplg_hdr
            "magic" / Const(Bytes(len(self._TPLG_MAGIC)), self._TPLG_MAGIC),
            "abi" / Const(Int32ul, self._abi),
            "version" / Int32ul,
            "type" / Enum(Int32ul, TplgType),
            "size" / Const(Int32ul, 4 * 9),
            "vendor_type" / Int32ul,
            "payload_size" / Int32ul,
            "index" / Int32ul,
            "count" / Int32ul,
        )
        self._vendor_uuid_elem = Struct( # snd_soc_tplg_vendor_uuid_elem
            "token" / Enum(Int32ul, SofVendorToken),
            "uuid" / Bytes(16),
        )
        self._vendor_value_elem = Struct( # snd_soc_tplg_vendor_value_elem
            "token" / Enum(Int32ul, SofVendorToken),
            "value" / Int32ul,
        )
        self._vendor_string_elem = Struct( # snd_soc_tplg_vendor_string_elem
            "token" / Enum(Int32ul, SofVendorToken),
            "string" / String(self._id_name_maxlen, "ascii")
        )
        self._vendor_elem_cases = {
            VendorTupleType.UUID.name: self._vendor_uuid_elem,
            VendorTupleType.STRING.name: self._vendor_string_elem,
            VendorTupleType.BOOL.name: self._vendor_value_elem,
            VendorTupleType.BYTE.name: self._vendor_value_elem,
            VendorTupleType.WORD.name: self._vendor_value_elem,
            VendorTupleType.SHORT.name: self._vendor_value_elem,
        }
        self._vendor_array = Struct( # snd_soc_tplg_vendor_array
            "size" / Int32ul, # size in bytes of the array, including all elements
            "type" / Enum(Int32ul, VendorTupleType),
            "num_elems" / Int32ul, # number of elements in array
            "elems" / Array(this.num_elems, Switch(this.type, self._vendor_elem_cases, default=construct.Error)),
        )
        # expand the union of snd_soc_tplg_private for different sections:
        self._private_raw = Prefixed(
            Int32ul, # size
            GreedyBytes, # data
        )
        # currently, only dapm_widget uses vendor array
        self._private_vendor_array = Prefixed(
            Int32ul, # size
            CompleteRange(self._vendor_array), # array
        )
        self._tlv_dbscale = Struct( # snd_soc_tplg_tlv_dbscale
            "min" / Int32ul,
            "step" / Int32ul,
            "mute" / Int32ul,
        )
        self._ctl_tlv = Struct( # snd_soc_tplg_ctl_tlv
            "size" / Int32ul,
            "type" / Enum(Int32ul, CtlTlvt),
            "scale" / Padded(4 * self._tlv_size, self._tlv_dbscale)
        )
        self._channel = Struct( # snd_soc_tplg_channel
            "size" / Int32ul,
            "reg" / Int32ul,
            "shift" / Int32ul,
            "id" / Int32ul,
        )
        self._io_ops = Struct( # snd_soc_tplg_io_ops
            "get" / Int32ul,
            "put" / Int32ul,
            "info" / Int32ul,
        )
        self._kcontrol_hdr = Struct( # snd_soc_tplg_ctl_hdr
            "size" / Int32ul,
            "type" / Enum(Int32ul, TplgType),
            "name" / String(self._id_name_maxlen, "ascii"),
            "access" / Int32ul,
            "ops" / self._io_ops,
            "tlv" / self._ctl_tlv,
        )
        self._stream_caps = Struct( # snd_soc_tplg_stream_caps
            "size" / Int32ul,
            "name" / String(self._id_name_maxlen, "ascii"),
            "formats" / FlagsEnum(Int64ul, PcmFormatsFlag),
            "rates" / Int32ul, # SNDRV_PCM_RATE_ ?
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
        self._stream = Struct( # snd_soc_tplg_stream
            "size" / Int32ul,
            "name" / String(self._id_name_maxlen, "ascii"),
            "format" / FlagsEnum(Int64ul, PcmFormatsFlag),
            "rate" / Int32ul,
            "period_bytes" / Int32ul,
            "buffer_bytes" / Int32ul,
            "channels" / Int32ul,
        )
        self._hw_config = Struct( # snd_soc_tplg_hw_config
            "size" / Int32ul,
            "id" / Int32ul,
            "fmt" / Enum(Int32ul, DaiFormat),
            "clock_gated" / Enum(Byte, DaiClockGate),
            "invert_bclk" / Flag,
            "invert_fsync" / Flag,
            "bclk_provider" / Enum(Byte, DaiBClk),
            "fsync_provider" / Enum(Byte, DaiFSync),
            "mclk_direction" / Enum(Byte, DaiMClk),
            Padding(2), # reserved
            "mclk_rate" / Int32ul,
            "bclk_rate" / Int32ul,
            "fsync_rate" / Int32ul,
            "tdm_slots" / Int32ul,
            "tdm_slot_width" / Int32ul,
            "tx_slots" / Int32ul,
            "rx_slots" / Int32ul,
            "tx_channels" / Int32ul,
            "tx_chanmap" / Array(self._max_channel, Int32ul),
            "rx_channels" / Int32ul,
            "rx_chanmap" / Array(self._max_channel, Int32ul),
        )
        self._manifest = Struct( # snd_soc_tplg_manifest
            "size" / Int32ul,
            "control_elems" / Int32ul,
            "widget_elems" / Int32ul,
            "graph_elems" / Int32ul,
            "pcm_elems" / Int32ul,
            "dai_link_elems" / Int32ul,
            "dai_elems" / Int32ul,
            Padding(4 * 20), # reserved
            "priv" / self._private_raw,
        )
        self._mixer_control_body = Struct( # `snd_soc_tplg_mixer_control` without `hdr`
            "size" / Int32ul,
            "min" / Int32ul,
            "max" / Int32ul,
            "platform_max" / Int32ul,
            "invert" / Int32ul,
            "num_channels" / Int32ul,
            "channel" / Array(self._max_channel, self._channel),
            "priv" / self._private_raw,
        )
        self._enum_control_body = Struct( # `snd_soc_tplg_enum_control` without `hdr`
            "size" / Int32ul,
            "num_channels" / Int32ul,
            "channel" / Array(self._max_channel, self._channel),
            "items" / Int32ul,
            "mask" / Int32ul,
            "count" / Int32ul,
            "texts" / Array(self._num_texts, String(self._id_name_maxlen, "ascii")),
            "values" / Array(self._num_texts * self._id_name_maxlen / 4, Int32ul),
            "priv" / self._private_raw,
        )
        self._bytes_control_body = Struct( # `snd_soc_tplg_bytes_control` without `hdr`
            "size" / Int32ul,
            "max" / Int32ul,
            "mask" / Int32ul,
            "base" / Int32ul,
            "num_regs" / Int32ul,
            "ext_ops" / self._io_ops,
            "priv" / self._private_raw,
        )
        self._kcontrol_cases = {
            TplgType.MIXER.name: self._mixer_control_body,
            TplgType.ENUM.name: self._enum_control_body,
            TplgType.BYTES.name: self._bytes_control_body,
        }
        self._kcontrol_wrapper = Struct( # wrapper for kcontrol types, divide header and body
            "hdr" / self._kcontrol_hdr,
            "body" / Switch(
                this.hdr.type,
                self._kcontrol_cases,
            ),
        )
        self._dapm_graph_elem = Struct( # snd_soc_tplg_dapm_graph_elem
            "sink" / String(self._id_name_maxlen, "ascii"),
            "control" / String(self._id_name_maxlen, "ascii"),
            "source" / String(self._id_name_maxlen, "ascii"),
        )
        self._dapm_widget = Struct( # snd_soc_tplg_dapm_widget
            "size" / Int32ul,
            "id" / Enum(Int32ul, DapmType),
            "name" / String(self._id_name_maxlen, "ascii"),
            "sname" / String(self._id_name_maxlen, "ascii"),
            "reg" / Int32ul,
            "shift" / Int32ul,
            "mask" / Int32ul,
            "subseq" / Int32ul,
            "invert" / Int32ul,
            "ignore_suspend" / Int32ul,
            "event_flags" / Int16ul,
            "event_type" / Int16ul,
            "num_kcontrols" / Int32ul,
            "priv" / self._private_vendor_array,
        )
        self._dapm_widget_with_kcontrols = Struct(
            "widget" / self._dapm_widget,
            "kcontrols" / Array(this.widget.num_kcontrols, self._kcontrol_wrapper),
        )
        self._pcm = Struct( # snd_soc_tplg_pcm
            "size" / Int32ul,
            "pcm_name" / String(self._id_name_maxlen, "ascii"),
            "dai_name" / String(self._id_name_maxlen, "ascii"),
            "pcm_id" / Int32ul,
            "dai_id" / Int32ul,
            "playback" / Int32ul,
            "capture" / Int32ul,
            "compress" / Int32ul,
            "stream" / Array(self._stream_config_max, self._stream),
            "num_streams" / Int32ul,
            "caps" / Array(2, self._stream_caps),
            "flag_mask" / Int32ul,
            "flags" / FlagsEnum(Int32ul, DaiLnkFlag),
            "priv" / self._private_raw,
        )
        self._link_config = Struct( # snd_soc_tplg_link_config
            "size" / Int32ul,
            "id" / Int32ul,
            "name" / String(self._id_name_maxlen, "ascii"),
            "stream_name" / String(self._id_name_maxlen, "ascii"),
            "stream" / Array(self._stream_config_max, self._stream),
            "num_streams" / Int32ul,
            "hw_config" / Array(self._hw_config_max, self._hw_config),
            "num_hw_configs" / Int32ul,
            "default_hw_config_id" / Int32ul,
            "flag_mask" / Int32ul,
            "flags" / FlagsEnum(Int32ul, DaiLnkFlag),
            "priv" / self._private_raw,
        )
        self._section_blocks_cases = {
            TplgType.MANIFEST.name: self._manifest,
            TplgType.MIXER.name: self._kcontrol_wrapper,
            TplgType.ENUM.name: self._kcontrol_wrapper,
            TplgType.BYTES.name: self._kcontrol_wrapper,
            TplgType.DAPM_GRAPH.name: self._dapm_graph_elem,
            TplgType.DAPM_WIDGET.name: self._dapm_widget_with_kcontrols,
            TplgType.PCM.name: self._pcm,
            TplgType.CODEC_LINK.name: self._link_config,
            TplgType.BACKEND_LINK.name: self._link_config,
        }
        _section_blocks_cases = dict((k, Array(this.header.count, v)) for (k, v) in self._section_blocks_cases.items())
        self._section = Struct(
            "header" / self._block_header,
            "blocks" / Switch(
                this.header.type,
                _section_blocks_cases,
                default = Padding(this.header.payload_size)  # skip unknown blocks,
            ),
        )
        self.sections = CompleteRange(self._section)

    def parse_file(self, filepath) -> ListContainer:
        "Parse the TPLG binary file to raw structure."
        with open(os.fspath(filepath), "rb") as fs:
            return self.sections.parse_stream(fs)

    def build(self, data: ListContainer) -> bytes:
        "Build the topology data to byte array."
        return self.sections.build(data)

@dataclass(init=False)
class GroupedTplg:
    "Grouped topology data."

    widget_list: "list[Container]"
    "DAPM widgets list."

    def __init__(self, raw_parsed_tplg: ListContainer):
        "Group topology blocks by their types."
        self.widget_list = []
        for item in raw_parsed_tplg:
            tplgtype = item["header"]["type"]
            blocks: ListContainer = item["blocks"]
            if tplgtype == TplgType.DAPM_WIDGET.name:
                self.widget_list.extend(blocks)

    @staticmethod
    def get_priv_element(comp: Container, token: SofVendorToken, default = None):
        "Get private element with specific token."
        try:
            return next(
                elem["uuid"]
                for vendor_array in comp["priv"]
                for elem in vendor_array["elems"]
                if elem["token"] == token.name
            )
        except (KeyError, AttributeError, StopIteration):
            return default

    @staticmethod
    def get_widget_uuid(widget: Container, default = None):
        "Get widget UUID."
        return GroupedTplg.get_priv_element(widget["widget"], SofVendorToken.SOF_TKN_COMP_UUID, default)

    def print_widget_uuid(self):
        for widget in self.widget_list:
            name = widget["widget"]["name"]
            uuid = self.get_widget_uuid(widget)
            if uuid is not None:
                print("widget:", name, uuid)

class Fw_Parser:

    def __init__(self):
        self.fw_header_offset = 0x2000

        self.Ext_header = Struct (
            "id" / String(4, "ascii"),
            "len" / Int32ul,
            "version_major" / Int16ul,
            "version_minor" / Int16ul,
            "module_entries" / Int32ul,
        )

        self.Fw_binary_header = Struct (
            "id" / String(4, "ascii"),
            "len" / Int32ul,
            "name" / String(8, "ascii"),
            "preload_page_count" / Int32ul,
            "flags" /Int32ul,
            "feature_mask" / Int32ul,
            "major" / Int16ul,
            "minor" / Int16ul,
            "hotfix" / Int16ul,
            "build" / Int16ul,
            "module_entries" / Int32ul,
            "hw_buffer_add" / Int32ul,
            "hw_buffer_len" / Int32ul,
            "load_offset" / Int32ul,
        )

        self.Module_segment = Struct (
            "flags" / Int32ul,
            "v_base_addr" / Int32ul,
            "file_offset" / Int32ul,
        )

        self.Module_entry = Struct (
            "id" / Int32ul,
            "name" / String(8, "ascii"),
            "uuid" / Bytes(16),
            "type" / Int32ul,
            "hash" / Bytes(32),
            "entry_point" / Int32ul,
            "cfg_offset" / Int16ul,
            "cfg_count" / Int16ul,
            "affinity_mask" / Int32ul,
            "instance_max_count" / Int16ul,
            "stack_size" / Int16ul,
            "segment" / self.Module_segment[3],
        )

        self.Ext_manifest = Struct (
            "ext_header" / self.Ext_header,
            "padding1" / Bytes(this.ext_header.len + self.fw_header_offset - self.Ext_header.sizeof()),
            "fw_binary_header" / self.Fw_binary_header,
            "modules" / self.Module_entry[this.fw_binary_header.module_entries],
        )

    def parse_file(self, filepath) -> ListContainer:
        with open(filepath, "rb") as fw :
            return self.Ext_manifest.parse_stream(fw)

if __name__ == "__main__":
    from pathlib import Path

    def parse_cmdline():
        parser = argparse.ArgumentParser(add_help=True, formatter_class=argparse.RawTextHelpFormatter,
            description='A Topology Reader totally Written in Python.')

        parser.add_argument('--version', action='version', version='%(prog)s 1.0')
        parser.add_argument('-t', type=str, nargs='+', required=True, help="""Load topology files for check """)
        parser.add_argument('-f', type=str, nargs='+', required=True, help="""Load firmware files for check """)
        parser.add_argument('-v', '--verbose', action="store_true", help="Show check state")

        return parser.parse_args()


    def main():
        tplgFormat = TplgBinaryFormat()

        cmd_args = parse_cmdline()

        tplg = GroupedTplg(tplgFormat.parse_file(cmd_args.t[0]))

        fw_parser = Fw_Parser()
        fw_content = fw_parser.parse_file(cmd_args.f[0])
        print("FW header", fw_content.ext_header, "\n")
        if fw_content.ext_header.id == '$AE1':
            print("IPC4 supported FW\n")
        elif fw_content.ext_header.id == 'XMan':
            print("IPC3 supported FW\n")
        else:
            print("Invalid FW")
            return

        not_found = 0
        for widget in tplg.widget_list:
            name = widget["widget"]["name"]
            uuid = tplg.get_widget_uuid(widget)
            found = False
            if uuid is not None:
                for m in fw_content.modules:
                    if m.uuid == uuid:
                        found = True
                        if cmd_args.verbose:
                            print("Widget: ", name, "is supported by ", m.name)
                        break;

                if found == False:
                    not_found += 1
                    print("Error: widget:", name, uuid, "is not supported by this FW")

        if not_found == 0:
            print("This FW can support with this topology")

    main()
