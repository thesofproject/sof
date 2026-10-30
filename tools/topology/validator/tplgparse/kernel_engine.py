# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2026 Intel Corporation. All rights reserved.

"""Emulation of the SOF driver vendor tuple engine.

Mirrors sof_parse_token_sets(), sof_copy_tuples(), sof_update_ipc_object(),
sof_get_token_value(), sof_widget_parse_tokens() and the token parsing part
of sof_widget_ready() in Linux sound/soc/sof/topology.c, driven by the
token tables generated into schema/kernel_tokens.py.

The point of emulating instead of decoding tuples directly is that the
kernel does not always read tuples the way the topology meant them:
sof_copy_tuples() picks the element layout from the type of the token it
is looking for rather than from the vendor array type, multiple token
sets are filled by occurrence count, and parsing stops once enough tokens
were found. Every value read is recorded with the private data offset it
came from so that checks can compare the kernel view to the intended one.
"""

import struct
from collections import Counter
from dataclasses import dataclass, field
from typing import Optional

from schema import kernel_tokens as kt

from .model import ARRAY_HDR_SIZE, ELEM_SIZE, TupleType

STRING_ELEM = ELEM_SIZE[TupleType.STRING]
VALUE_ELEM = ELEM_SIZE[TupleType.WORD]
UUID_ELEM = ELEM_SIZE[TupleType.UUID]
NAME_LEN = STRING_ELEM - 4

WORD_TYPES = ("WORD", "SHORT", "BYTE", "BOOL")

SOF_WIDGET_MAX_NUM_PINS = 8  # sound/soc/sof/sof-audio.h

TKN_NUM_INPUT_PINS = 411
TKN_NUM_OUTPUT_PINS = 412
TKN_NUM_INPUT_AUDIO_FORMATS = 415
TKN_NUM_OUTPUT_AUDIO_FORMATS = 416


class KernelError(Exception):
    "The kernel would fail topology load at this point."

    def __init__(self, message, kernel_ref):
        super().__init__(message)
        self.kernel_ref = kernel_ref


@dataclass
class Read:
    "One value the kernel picked up from private data."
    token: int
    kind: str  # 'string', 'value' or 'uuid': the element layout the kernel used
    value: object
    offset: int  # element offset within the private data
    oob: bool = False  # element lies (partly) outside its vendor array


@dataclass
class Trace:
    "Everything the emulation observed while parsing one private data blob."
    reads: list = field(default_factory=list)  # [Read], values actually used
    oob_reads: list = field(default_factory=list)  # [(offset, length, priv_size)]


def _u32(data, off):
    return struct.unpack_from("<I", data, off)[0]


def _string(data, off):
    raw = data[off:off + NAME_LEN]
    return raw.split(b"\0", 1)[0].decode("ascii", errors="replace")


def _elem(data, arr_off, asize, kind, i, trace):
    """Read element i of a vendor array with the given layout.

    Returns (token, value, offset, oob) or None if the read would go past
    the private data, which the emulation cannot follow.
    """
    esize = {"string": STRING_ELEM, "value": VALUE_ELEM, "uuid": UUID_ELEM}[kind]
    off = arr_off + ARRAY_HDR_SIZE + i * esize
    oob = off + esize > arr_off + asize
    if off + esize > len(data):
        trace.oob_reads.append((off, esize, len(data)))
        return None
    token = _u32(data, off)
    if kind == "string":
        value = _string(data, off + 4)
    elif kind == "uuid":
        value = bytes(data[off + 4:off + 20])
    else:
        value = _u32(data, off + 4)
    return token, value, off, oob


def parse_token_sets(data: bytes, tokens, instances: int, trace: Trace,
                     ref="sof_parse_token_sets") -> list:
    """sof_parse_token_sets(): returns a list of sets, each {token: value}.

    tokens is a list of (token, type, getter) from the generated schema.
    """
    count = len(tokens)
    sets = [{}]
    found = total = 0
    array_size = len(data)
    pos = 0

    while array_size > 0 and total < count * instances:
        if array_size < ARRAY_HDR_SIZE:
            raise KernelError("vendor array header does not fit in private data", ref)
        asize, atype, num = struct.unpack_from("<iII", data, pos)
        if asize < ARRAY_HDR_SIZE:
            raise KernelError(f"invalid vendor array size 0x{asize:x}", ref)
        array_size -= asize
        if array_size < 0:
            raise KernelError(f"invalid vendor array size 0x{asize:x}", ref)

        try:
            ttype = TupleType(atype)
        except ValueError:
            raise KernelError(f"unknown token type {atype}", ref) from None

        if ttype == TupleType.UUID:
            kind, match = "uuid", ("UUID",)
        elif ttype == TupleType.STRING:
            kind, match = "string", ("STRING",)
        else:
            kind, match = "value", WORD_TYPES

        for i in range(num):
            e = _elem(data, pos, asize, kind, i, trace)
            if e is None:
                break
            token, value, off, oob = e
            for tok, typ, _getter in tokens:
                if typ in match and tok == token:
                    sets[-1][tok] = value
                    trace.reads.append(Read(tok, kind, value, off, oob))
                    found += 1

        pos += asize
        if found >= count:
            sets.append({})
            total += found
            found = 0

    if not sets[-1]:
        sets.pop()
    return sets


def parse_tokens(data: bytes, tokens, trace: Trace, ref="sof_parse_tokens") -> dict:
    "sof_parse_tokens(): object_size is 0, so every set lands on the same object."
    merged = {}
    for s in parse_token_sets(data, tokens, 1, trace, ref):
        merged.update(s)
    return merged


@dataclass
class Tuple:
    "struct snd_sof_tuple"
    token: int
    value: object


def copy_tuples(data: bytes, tokens, instances: int, tuples: list, tuples_size: int,
                trace: Trace, ref="sof_copy_tuples"):
    """sof_copy_tuples(): append matching tuples to the tuples list.

    Element layout is chosen by the kernel token type, not by the vendor
    array type, exactly as the kernel does it.
    """
    if len(tuples) >= tuples_size:
        raise KernelError("no space in tuples array for new tokens", ref)

    num_tokens = len(tokens)
    found = 0
    array_size = len(data)
    pos = 0

    while array_size > 0 and found < num_tokens * instances:
        if array_size < ARRAY_HDR_SIZE:
            # the kernel reads the header anyway; we cannot follow it
            trace.oob_reads.append((pos, ARRAY_HDR_SIZE, len(data)))
            return
        asize, _atype, num = struct.unpack_from("<iII", data, pos)
        if asize < 0:
            raise KernelError(f"invalid vendor array size 0x{asize:x}", ref)
        array_size -= asize
        if array_size < 0:
            raise KernelError(f"invalid vendor array size 0x{asize:x}", ref)

        # Elements past the end of the private data cannot be read by the
        # emulation, so do not walk a bogus num_elems. Using the smallest
        # element size keeps reads across vendor array boundaries, which
        # the kernel does as well.
        readable = max(0, (len(data) - pos - ARRAY_HDR_SIZE) // VALUE_ELEM)
        if num > readable:
            trace.oob_reads.append((pos + ARRAY_HDR_SIZE + readable * VALUE_ELEM,
                                    (num - readable) * VALUE_ELEM, len(data)))
        for i in range(min(num, readable)):
            for tok, typ, _getter in tokens:
                if typ not in WORD_TYPES and typ != "STRING":
                    continue
                kind = "string" if typ == "STRING" else "value"
                e = _elem(data, pos, asize, kind, i, trace)
                if e is None:
                    continue
                token, value, off, oob = e
                if token != tok:
                    continue
                tuples.append(Tuple(tok, value))
                trace.reads.append(Read(tok, kind, value, off, oob))
                found += 1
                if len(tuples) == tuples_size:
                    return
            if found == num_tokens * instances:
                return

        if asize == 0:
            # the kernel would loop forever on the same array
            raise KernelError("zero sized vendor array", ref)
        pos += asize


def get_token_value(token: int, tuples: list) -> int:
    "sof_get_token_value(), returns None where the kernel returns -EINVAL."
    for t in tuples:
        if t.token == token:
            return t.value
    return None


def update_ipc_object(tuples: list, tokens, instances: int) -> list:
    """sof_update_ipc_object(): returns sets of {token: value}.

    The n-th occurrence of a token lands in set n, regardless of which
    format block it came from in the topology. instances comes from the
    topology, so only the sets that can receive a value are returned:
    at most `instances`, and no more than the most frequent token occurs.
    The kernel leaves the remaining sets zeroed.
    """
    occurrences = Counter(t.token for t in tuples)
    most = max((occurrences[tok] for tok, _typ, _g in tokens), default=0)
    sets = [{} for _ in range(max(min(instances, most), 1))]
    for tok, typ, _getter in tokens:
        idx = 0
        matched = 0
        for t in tuples:
            if t.token != tok:
                continue
            value = t.value
            if typ in ("SHORT", "BOOL") and isinstance(value, int):
                value &= 0xffff
            if idx < len(sets):
                sets[idx].setdefault(tok, value)
            matched += 1
            if not matched % max(instances, 1):
                break
            idx += 1
    return sets


def group(name: str):
    "Token table of an IPC4 token group, or [] if IPC4 does not define it."
    entry = kt.IPC4_TOKEN_LIST.get(name)
    return entry[1] if entry else []


@dataclass
class KernelWidget:
    "What sof_widget_ready() and sof_widget_parse_tokens() extract."
    num_input_pins: int = 0
    num_output_pins: int = 0
    input_pin_binding: Optional[list] = None
    output_pin_binding: Optional[list] = None
    no_wname_in_kcontrol_name: bool = False
    comp_ext: dict = field(default_factory=dict)  # token -> value
    tuples: list = field(default_factory=list)  # [Tuple]
    trace: Trace = field(default_factory=Trace)
    error: Optional[KernelError] = None
    token_groups: list = field(default_factory=list)

    def value(self, token: int, default=None):
        v = get_token_value(token, self.tuples)
        return default if v is None else v

    def update(self, group_name: str, instances: int = 1) -> list:
        return update_ipc_object(self.tuples, group(group_name), instances)

    def audio_formats(self):
        """sof_ipc4_get_audio_fmt(): (num_in, num_out, in_sets, out_sets).

        num_in and num_out are the declared counts. The format lists may be
        shorter when the topology does not define that many formats.
        """
        num = self.update("SOF_AUDIO_FMT_NUM_TOKENS")[0]
        n_in = num.get(TKN_NUM_INPUT_AUDIO_FORMATS, 0)
        n_out = num.get(TKN_NUM_OUTPUT_AUDIO_FORMATS, 0)
        ins = self.update("SOF_IN_AUDIO_FORMAT_TOKENS", n_in)[:n_in] if n_in else []
        outs = self.update("SOF_OUT_AUDIO_FORMAT_TOKENS", n_out)[:n_out] if n_out else []
        return n_in, n_out, ins, outs


def widget_ready(widget) -> KernelWidget:
    """Token parsing done by sof_widget_ready() for an IPC4 topology.

    Stops at the first point where the kernel would return an error and
    records it in KernelWidget.error.
    """
    kw = KernelWidget()
    data = widget.priv.data
    gt = kt.GENERIC_TABLES
    try:
        d = parse_tokens(data, gt["dapm_widget_tokens"], kw.trace)
        kw.no_wname_in_kcontrol_name = bool(d)

        pins = parse_tokens(data, gt["comp_pin_tokens"], kw.trace)
        kw.num_input_pins = pins.get(TKN_NUM_INPUT_PINS, 0)
        kw.num_output_pins = pins.get(TKN_NUM_OUTPUT_PINS, 0)
        if kw.num_input_pins > SOF_WIDGET_MAX_NUM_PINS or \
           kw.num_output_pins > SOF_WIDGET_MAX_NUM_PINS:
            raise KernelError(f"invalid pins: input {kw.num_input_pins}, "
                              f"output {kw.num_output_pins}, max {SOF_WIDGET_MAX_NUM_PINS}",
                              "sof_widget_ready")

        for attr, num, table in (
                ("input_pin_binding", kw.num_input_pins, "comp_input_pin_binding_tokens"),
                ("output_pin_binding", kw.num_output_pins, "comp_output_pin_binding_tokens")):
            if num > 1:
                sets = parse_token_sets(data, gt[table], num, kw.trace, "sof_parse_pin_binding")
                binding = [None] * num
                for i, s in enumerate(sets[:num]):
                    binding[i] = next(iter(s.values()), None)
                if binding[0] is not None:
                    setattr(kw, attr, binding)

        _setup, token_list = kt.IPC4_WIDGET_TOKEN_LISTS.get(widget.type_name, (None, []))
        kw.token_groups = list(token_list)
        widget_parse_tokens(data, token_list, kw)
    except KernelError as e:
        kw.error = e
    return kw


def widget_parse_tokens(data: bytes, token_list, kw: KernelWidget):
    "sof_widget_parse_tokens()"
    ref = "sof_widget_parse_tokens"
    num_tuples = sum(len(group(g)) for g in token_list)

    for g in token_list:
        num_sets = 1
        if g == "SOF_COMP_EXT_TOKENS":
            kw.comp_ext = parse_tokens(data, group(g), kw.trace, ref)
            continue
        if g == "SOF_IN_AUDIO_FORMAT_TOKENS":
            num_sets = get_token_value(TKN_NUM_INPUT_AUDIO_FORMATS, kw.tuples)
            if num_sets is None:
                raise KernelError("invalid input audio format count", ref)
        elif g == "SOF_OUT_AUDIO_FORMAT_TOKENS":
            num_sets = get_token_value(TKN_NUM_OUTPUT_AUDIO_FORMATS, kw.tuples)
            if num_sets is None:
                raise KernelError("invalid output audio format count", ref)

        if num_sets > 1:
            num_tuples += len(group(g)) * (num_sets - 1)

        if not group(g):
            continue  # IPC4 defines no tokens for this group
        copy_tuples(data, group(g), num_sets, kw.tuples, num_tuples, kw.trace, ref)


def private_tokens(priv, table_name: str) -> dict:
    "sof_parse_tokens() with one of the generic tables, for PCM/link/control data."
    trace = Trace()
    try:
        return parse_tokens(priv.data, kt.GENERIC_TABLES[table_name], trace)
    except KernelError:
        return {}
