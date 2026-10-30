# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2026 Intel Corporation. All rights reserved.

"""L3: IPC4 load time and pipeline setup rules.

Mirrors sound/soc/sof/topology.c (sof_widget_ready, sof_route_load,
sof_complete, sof_link_load, spcm_bind) and sound/soc/sof/ipc4-topology.c
(the ipc_setup callbacks, control setup, route setup, manifest parsing).
"""

import struct
from collections import Counter

from schema import kernel_tokens as kt
from tplgparse import binary
from tplgparse.binary import TplgType

from . import ERROR, INFO, WARNING, rule
from .paths import host_copier, routes_on_pcm_paths

VIRTUAL_TYPES = ("input", "output", "out_drv")  # skipped by sof_route_setup()
AUDIO_FMT_TYPES = ("aif_in", "aif_out", "dai_in", "dai_out", "buffer", "pga", "mixer",
                   "src", "asrc", "effect", "decoder", "encoder")
PROCESS_TYPES = ("effect", "decoder", "encoder")
DAI_WIDGET_TYPES = ("dai_in", "dai_out")
IPC4_DAI_CONFIG_TYPES = ("SOF_DAI_INTEL_HDA", "SOF_DAI_INTEL_ALH", "SOF_DAI_INTEL_DMIC",
                         "SOF_DAI_INTEL_SSP")  # sof_ipc4_dai_config()
CHAIN_DMA_DAI_TYPES = ("SOF_DAI_INTEL_HDA",)  # hda_is_chain_dma_supported()

TKN_SCHED_USE_CHAIN_DMA = 209
TKN_SCHED_DYNAMIC_PIPELINE = 206
TKN_SCHED_CORE = 203
TKN_COMP_CORE_ID = 404
TKN_COMP_UUID = 405
TKN_DAI_TYPE = 154
TKN_IN_PIN_INDEX = 1909
TKN_OUT_PIN_INDEX = 1939

SNDRV_CTL_ELEM_ACCESS_VOLATILE = 1 << 2
SND_SOC_TPLG_CTL_VOLSW, SND_SOC_TPLG_CTL_VOLSW_SX, SND_SOC_TPLG_CTL_VOLSW_XR_SX = 1, 2, 3
SND_SOC_TPLG_CTL_ENUM, SND_SOC_TPLG_CTL_BYTES, SND_SOC_TPLG_CTL_ENUM_VALUE = 4, 5, 6
VOLUME_INFO = (SND_SOC_TPLG_CTL_VOLSW, SND_SOC_TPLG_CTL_VOLSW_SX, SND_SOC_TPLG_CTL_VOLSW_XR_SX)
ENUM_INFO = (SND_SOC_TPLG_CTL_ENUM, SND_SOC_TPLG_CTL_ENUM_VALUE)
MAX_CHAN = 8
INT_MAX = 0x7fffffff

SOF_ABI_HDR_SIZE = 32  # struct sof_abi_hdr
SOF_IPC4_ABI_MAGIC = 0x34464F53
SOF_IPC4_TPLG_ABI_SIZE = 6
SOF_IPC4_MANIFEST_SIZE = 8  # struct sof_manifest without items
SOF_MANIFEST_DATA_TYPE_NHLT = 1


def pipeline_info(ctx, sched):
    """(use_chain_dma, dynamic) of a scheduler widget as set by
    sof_ipc4_widget_setup_comp_pipeline(). With chain DMA the function
    returns before parsing SOF_PIPELINE_TOKENS, so dynamic stays 0."""
    kw = ctx.kw(sched)
    chain = bool(kw.update("SOF_SCHED_TOKENS")[0].get(TKN_SCHED_USE_CHAIN_DMA, 0))
    dynamic = False if chain else \
        bool(kw.update("SOF_PIPELINE_TOKENS")[0].get(TKN_SCHED_DYNAMIC_PIPELINE, 0))
    return chain, dynamic


def dai_type(ctx, widget):
    "SOF_DAI_* name of a DAI copier, via find_dai()."
    name = ctx.kw(widget).update("SOF_DAI_TOKENS")[0].get(TKN_DAI_TYPE)
    return name, kt.DAI_TYPES.get(name, "SOF_DAI_INTEL_NONE")


def kernel_routes(ctx):
    """Routes that can reach sof_ipc4_route_setup(): both ends exist and are
    IPC4 modules. Virtual widgets are skipped by sof_route_setup(), widgets
    without IPC4 widget ops (e.g. siggen) have no firmware module."""
    t = ctx.tplg
    for r in t.routes:
        src, sink = t.widget(r.source), t.widget(r.sink)
        if src and sink and src.type_name in kt.IPC4_WIDGET_TOKEN_LISTS and \
           sink.type_name in kt.IPC4_WIDGET_TOKEN_LISTS:
            yield r, src, sink


# --- graph and pipelines -------------------------------------------------

@rule("G001", "L3", ERROR, "snd_soc_dapm_add_routes, sof_route_load",
      "route source and sink widgets exist")
def route_endpoints(ctx):
    t = ctx.tplg
    for r in t.routes:
        for end, name in (("source", r.source), ("sink", r.sink)):
            if t.widget(name) is None:
                hint = "" if t.base_widgets else \
                    " (for a feature topology, give the topologies loaded before it with --base)"
                yield r.label, f"{end} widget '{name}' does not exist{hint}"


def enum_texts(control) -> list:
    "Enum control entries, as matched by dapm_connect_mux()."
    items = min(control.body["items"], len(control.body.texts))
    return [binary.decode_name(txt) for txt in control.body.texts[:items]]


@rule("G002", "L3", ERROR, "snd_soc_dapm_add_path, dapm_connect_mux, dapm_connect_mixer",
      "route controls match the sink widget the way DAPM connects them")
def route_controls(ctx):
    t = ctx.tplg
    for r in t.routes:
        sink = t.widget(r.sink)
        if not r.control or sink is None:
            continue
        if sink.type_name == "mux":
            # the control names an entry of the mux's first enum control
            enum = sink.kcontrols[0] if sink.kcontrols else None
            if enum is None or enum.kind != TplgType.ENUM:
                yield r.label, "mux sink has no enum kcontrol to connect the route with"
            elif r.control not in enum_texts(enum):
                yield r.label, f"'{r.control}' is not an entry of mux control '{enum.name}'"
        elif sink.type_name in ("mixer", "switch"):
            if r.control not in [k.name for k in sink.kcontrols]:
                yield r.label, f"{sink.type_name} sink has no kcontrol '{r.control}'"
        # DAPM ignores the control for other sink types


@rule("G003", "L3", WARNING, "snd_soc_dapm_new_controls",
      "widget names are unique")
def duplicate_widgets(ctx):
    t = ctx.tplg
    for name, n in Counter(w.name for w in t.widgets).items():
        if n > 1:
            yield f"widget '{name}'", f"defined {n} times, routes resolve to the first one"
    base = {w.name for w in t.base_widgets}
    for w in t.widgets:
        if w.name in base:
            yield w.label, "also defined by a base topology"


@rule("G004", "L3", WARNING, "sof-audio.c pipeline walk",
      "every non-scheduler widget is connected to a route")
def unconnected(ctx):
    t = ctx.tplg
    routed = {n for r in t.routes for n in (r.source, r.sink)}
    for w in t.widgets:
        if w.type_name != "scheduler" and w.name not in routed:
            yield w.label, "not connected to any route"


@rule("G005", "L3", ERROR, "sof_complete, sof_widget_setup_unlocked",
      "every widget has a scheduler widget with the same pipeline index")
def widget_pipeline(ctx):
    t = ctx.tplg
    scheds = Counter(w.index for w in t.widgets if w.type_name == "scheduler")
    base_scheds = {w.index for w in t.base_widgets if w.type_name == "scheduler"}
    for w in t.widgets:
        if w.type_name == "scheduler":
            if scheds[w.index] > 1:
                yield w.label, f"{scheds[w.index]} scheduler widgets share pipeline " \
                               f"index {w.index}", WARNING
        elif w.index not in scheds and w.index not in base_scheds and \
                w.type_name in kt.IPC4_WIDGET_TOKEN_LISTS:
            # only widgets with IPC4 widget ops are set up in the firmware,
            # DAPM-only widgets like mux, adc and dac need no pipeline
            yield w.label, (f"no scheduler widget for pipeline index {w.index}; the widget "
                            f"is never set up")


@rule("G006", "L3", ERROR, "sof_ipc4_route_setup",
      "chain DMA is enabled on both pipelines of a route or on neither")
def chain_dma_routes(ctx):
    for r, src, sink in kernel_routes(ctx):
        ps, pk = ctx.pipeline_widget(src), ctx.pipeline_widget(sink)
        if ps is None or pk is None or ps is pk:
            continue
        cs, ck = pipeline_info(ctx, ps)[0], pipeline_info(ctx, pk)[0]
        if cs != ck:
            yield r.label, (f"use_chain_dma is {int(cs)} on '{ps.name}' but {int(ck)} "
                            f"on '{pk.name}'")


@rule("G007", "L3", ERROR, "sof_set_widget_pipeline",
      "widgets in dynamic pipelines have no volatile kcontrols")
def dynamic_volatile(ctx):
    for w in ctx.tplg.widgets:
        sched = ctx.pipeline_widget(w)
        if sched is None or w is sched or not pipeline_info(ctx, sched)[1]:
            continue
        for k in w.kcontrols:
            if k.hdr.access & SNDRV_CTL_ELEM_ACCESS_VOLATILE:
                yield w.label, f"volatile {k.label} in dynamic pipeline '{sched.name}'"


# --- widgets ---------------------------------------------------------------

@rule("W001", "L3", ERROR, "sof_widget_ready",
      "process widgets have private data, volume widgets have a kcontrol")
def widget_basics(ctx):
    for w in ctx.tplg.widgets:
        if w.type_name in PROCESS_TYPES and not w.priv.data:
            yield w.label, "process tokens not found (empty private data)"
        if w.type_name == "pga" and not w.raw.num_kcontrols:
            yield w.label, "volume widget without kcontrols"


@rule("W002", "L3", ERROR, "sof_ipc4_get_audio_fmt, sof_ipc4_widget_setup_comp_src",
      "modules declare audio formats; SRC/ASRC have input and output formats")
def audio_format_counts(ctx):
    for w in ctx.tplg.widgets:
        kw = ctx.kw(w)
        if kw.error or w.type_name not in AUDIO_FMT_TYPES or ctx.pipeline_widget(w) is None:
            continue
        n_in, n_out, _, _ = kw.audio_formats()
        if not n_in and not n_out:
            yield w.label, "no input/output pin formats set in topology"
        elif w.type_name in ("src", "asrc") and (not n_in or not n_out):
            yield w.label, f"{w.type_name} needs at least one input and one output format " \
                           f"(has {n_in} and {n_out})"


@rule("W003", "L3", ERROR, "sof_ipc4_widget_set_module_info",
      "module widgets have a UUID to look up the firmware module")
def module_uuid(ctx):
    for w in ctx.tplg.widgets:
        if w.type_name not in AUDIO_FMT_TYPES or ctx.pipeline_widget(w) is None:
            continue
        uuid = ctx.kw(w).comp_ext.get(TKN_COMP_UUID)
        if not uuid or not any(uuid):
            yield w.label, "no module UUID, the firmware module lookup will fail"


@rule("W004", "L3", ERROR, "find_dai, sof_ipc4_dai_config",
      "DAI copiers have a DAI type supported by the IPC4 driver")
def dai_types(ctx):
    for w in ctx.tplg.widgets:
        if w.type_name not in DAI_WIDGET_TYPES or ctx.kw(w).error:
            continue
        name, dtype = dai_type(ctx, w)
        if dtype == "SOF_DAI_INTEL_NONE":
            yield w.label, f"unknown DAI type {name!r}"
        elif dtype not in IPC4_DAI_CONFIG_TYPES:
            yield w.label, f"DAI type {name} ({dtype}) is not supported by the IPC4 driver"


@rule("W005", "L3", ERROR, "sof_ipc4_widget_setup_comp_dai, snd_sof_is_chain_dma_supported",
      "chain DMA pipelines only use DAI types supporting chain DMA")
def chain_dma_dai(ctx):
    for w in ctx.tplg.widgets:
        if w.type_name not in DAI_WIDGET_TYPES or ctx.kw(w).error:
            continue
        sched = ctx.pipeline_widget(w)
        if sched is None or not pipeline_info(ctx, sched)[0]:
            continue
        name, dtype = dai_type(ctx, w)
        if dtype not in CHAIN_DMA_DAI_TYPES:
            yield w.label, f"chain DMA is not supported with DAI type {name}"


@rule("W006", "L3", WARNING, "sof_widget_ready",
      "core IDs are within the DSP core count (--num-cores)")
def core_ids(ctx):
    n = ctx.options.num_cores
    if not n:
        return
    for w in ctx.tplg.widgets:
        kw = ctx.kw(w)
        if w.type_name == "scheduler":
            core = kw.update("SOF_SCHED_TOKENS")[0].get(TKN_SCHED_CORE)
        else:
            core = kw.comp_ext.get(TKN_COMP_CORE_ID)
        if core is not None and core >= n:
            yield w.label, f"core {core} does not exist on a {n} core DSP, the kernel " \
                           f"moves it to core 0"


@rule("W007", "L3", ERROR, "sof_parse_pin_binding, sof_ipc4_get_queue_id",
      "pin bindings are complete and match the routes")
def pin_bindings(ctx):
    t = ctx.tplg
    for w in t.widgets:
        kw = ctx.kw(w)
        for side, binding in (("input", kw.input_pin_binding), ("output", kw.output_pin_binding)):
            if binding is None:
                continue
            for i, name in enumerate(binding):
                if name is None:
                    yield w.label, f"{side} pin {i} has no binding, the kernel " \
                                   f"dereferences NULL when matching routes"
                elif t.widget(name) is None:
                    yield w.label, f"{side} pin {i} is bound to unknown widget '{name}'", \
                        WARNING

    # queue IDs are allocated when a route is set up for a PCM, which depends
    # on the DAPM walk; the path model is approximate, so these are warnings
    on_path = routes_on_pcm_paths(ctx)
    fan = Counter()
    for r, src, sink in kernel_routes(ctx):
        sev = WARNING
        pcms = on_path.get(id(r))
        when = f" (fails when PCM {', '.join(pcms)} is opened)" if pcms else ""
        ps, pk = ctx.pipeline_widget(src), ctx.pipeline_widget(sink)
        if ps is None or pk is None or pipeline_info(ctx, ps)[0] or pipeline_info(ctx, pk)[0]:
            continue  # no binding for chain DMA routes
        for side, w, peer in (("output", src, sink), ("input", sink, src)):
            kw = ctx.kw(w)
            pins = kw.num_output_pins if side == "output" else kw.num_input_pins
            binding = kw.output_pin_binding if side == "output" else kw.input_pin_binding
            fan[(w.name, side)] += 1
            if pins < 1:
                yield r.label, f"'{w.name}' has {pins} {side} pins{when}", sev
            elif pins > 1 and binding is not None and peer.name not in binding:
                yield r.label, f"'{peer.name}' is not in the {side} pin binding of " \
                               f"'{w.name}'{when}", sev

    for (name, side), n in fan.items():
        kw = ctx.kw(t.widget(name))
        pins = kw.num_output_pins if side == "output" else kw.num_input_pins
        if pins >= 1 and n > pins:
            yield f"widget '{name}'", f"{n} {side} routes but only {pins} {side} pins", WARNING


@rule("W008", "L3", WARNING, "sof_ipc4_process_set_pin_formats",
      "process modules with several pins have a format for every pin index")
def process_pin_formats(ctx):
    for w in ctx.tplg.widgets:
        kw = ctx.kw(w)
        if w.type_name not in PROCESS_TYPES or kw.error:
            continue
        _, _, ins, outs = kw.audio_formats()
        for side, pins, fmts, tok in (("input", kw.num_input_pins, ins, TKN_IN_PIN_INDEX),
                                      ("output", kw.num_output_pins, outs, TKN_OUT_PIN_INDEX)):
            have = {f.get(tok, 0) for f in fmts}
            missing = [i for i in range(1, pins) if i not in have]
            if missing:
                yield w.label, (f"no {side} format for pin {', '.join(map(str, missing))}; "
                                f"required if the firmware module uses extended init")


# --- controls ----------------------------------------------------------------

@rule("C001", "L3", ERROR, "sof_control_load_volume, sof_control_load_enum",
      "volume and enum controls have valid channel counts, ranges and TLV")
def volume_enum_controls(ctx):
    for c in ctx.tplg.controls:
        info = c.hdr.ops.info
        if c.kind == TplgType.MIXER and info in VOLUME_INFO:
            b = c.body
            lo, hi = b.min & 0xffffffff, b.max & 0xffffffff  # compared as u32
            if b.num_channels > MAX_CHAN:
                yield c.label, f"{b.num_channels} channels, maximum is {MAX_CHAN}"
            if lo > hi or hi >= INT_MAX:
                yield c.label, f"invalid range min {lo} max {hi}"
            elif hi != 1:
                # only max == 1 (a switch) skips the volume table, and
                # soc_tplg_create_tlv() only sets kc->tlv.p for dB scale TLVs
                access = c.hdr.access
                if not access & 0x30 or access & (1 << 28) or c.hdr.tlv.type != 1:
                    yield c.label, f"max {hi} needs a dB scale TLV for the volume table"
        elif c.kind == TplgType.ENUM and info in ENUM_INFO and c.body.num_channels > MAX_CHAN:
            yield c.label, f"{c.body.num_channels} channels, maximum is {MAX_CHAN}"


@rule("C002", "L3", ERROR, "sof_ipc4_control_load_bytes",
      "bytes controls have a valid size and IPC4 ABI header")
def bytes_controls(ctx):
    for c in ctx.tplg.controls:
        if c.kind != TplgType.BYTES or c.hdr.ops.info != SND_SOC_TPLG_CTL_BYTES:
            continue
        max_size, data = c.body.max, c.priv.data
        if max_size < SOF_ABI_HDR_SIZE:
            yield c.label, f"max size {max_size} is less than the ABI header"
            continue
        if len(data) > max_size:
            yield c.label, f"initial data size {len(data)} exceeds max {max_size}"
            continue
        if not data:
            continue
        if len(data) < SOF_ABI_HDR_SIZE:
            yield c.label, f"initial data size {len(data)} is less than the ABI header"
            continue
        magic, _type, size = struct.unpack_from("<III", data)
        if magic != SOF_IPC4_ABI_MAGIC:
            yield c.label, f"wrong ABI magic 0x{magic:x}, IPC4 needs 0x{SOF_IPC4_ABI_MAGIC:x}"
        elif size + SOF_ABI_HDR_SIZE != len(data):
            yield c.label, (f"ABI header size {size} + {SOF_ABI_HDR_SIZE} does not match "
                            f"data size {len(data)}")


# --- PCMs, links and manifest ------------------------------------------------

@rule("P001", "L3", ERROR, "spcm_bind, snd_sof_find_swidget_sname",
      "each PCM direction binds to a host copier by stream name")
def pcm_bind(ctx):
    t = ctx.tplg
    for p in t.pcms:
        for d, (enabled, wtype) in enumerate(((p.raw.playback, "aif_in"),
                                              (p.raw.capture, "aif_out"))):
            if not enabled:
                continue
            name = p.raw.caps[d].name.split(b"\0", 1)[0].decode("ascii", "replace")
            if host_copier(t, d, name) is None:
                yield p.label, (f"no {wtype} widget with stream name '{name}' for "
                                f"{('playback', 'capture')[d]}")


@rule("P002", "L3", WARNING, "sof_connect_dai_widget, soc_tplg_link_config",
      "DAI copiers and BE link configs refer to the same links")
def dai_links(ctx):
    t = ctx.tplg
    links = {n for link in t.links for n in (link.name, link.stream_name) if n}
    dai_snames = set()
    for w in t.widgets:
        if w.type_name in DAI_WIDGET_TYPES:
            dai_snames.add(w.sname)
            if not w.sname:
                yield w.label, "DAI widget has no stream name", ERROR
            elif w.sname not in links:
                yield w.label, (f"stream name '{w.sname}' matches no link config in the "
                                f"topology; the machine driver must provide this BE link")
    for link in t.links:
        if link.section_type == TplgType.BACKEND_LINK and link.name not in dai_snames and \
           link.stream_name not in dai_snames:
            yield link.label, "no DAI widget uses this link"


@rule("L001", "L3", ERROR, "sof_link_load, soc_tplg_link_config",
      "BE link configs have DAI tokens and valid hw configs")
def be_links(ctx):
    for link in ctx.tplg.links:
        if link.section_type != TplgType.BACKEND_LINK:
            continue
        if not link.priv.data:
            yield link.label, "expected tokens for DAI, none found"
        n = link.raw.num_hw_configs
        if n > 8:
            yield link.label, f"{n} hw configs, maximum is 8"
            continue
        ids = [hw.id for hw in link.hw_configs]
        if n and link.raw.default_hw_config_id not in ids:
            yield link.label, (f"default hw config id {link.raw.default_hw_config_id} is not "
                               f"one of {ids}, no DAI format is applied"), INFO


@rule("M001", "L3", ERROR, "sof_ipc4_parse_manifest",
      "manifest private data has the ABI version and well formed TLVs")
def manifest(ctx):
    m = ctx.tplg.manifest
    if m is None:
        return
    data = m.data.priv
    if len(data) < SOF_IPC4_TPLG_ABI_SIZE:
        yield "manifest", f"private data of {len(data)} bytes has no ABI version"
        return
    if len(data) <= SOF_IPC4_TPLG_ABI_SIZE:
        return
    if len(data) < SOF_IPC4_MANIFEST_SIZE:
        # the kernel reads the TLV count past the end of the private data
        yield "manifest", (f"private data of {len(data)} bytes has no complete TLV count, "
                           f"{SOF_IPC4_MANIFEST_SIZE} bytes needed")
        return
    _maj, _min, _patch, count = struct.unpack_from("<HHHH", data)
    pos = SOF_IPC4_MANIFEST_SIZE
    for i in range(count):
        if pos + 8 > len(data):
            yield "manifest", f"TLV {i} header is past the end of the private data"
            return
        typ, size = struct.unpack_from("<II", data, pos)
        pos += 8 + size
        if pos > len(data):
            yield "manifest", f"TLV {i} of {size} bytes is past the end of the private data"
            return
        if typ != SOF_MANIFEST_DATA_TYPE_NHLT:
            yield "manifest", f"TLV {i} has unknown type {typ}, the kernel skips it", WARNING
