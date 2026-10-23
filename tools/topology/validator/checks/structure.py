# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2026 Intel Corporation. All rights reserved.

"""L1: binary structure as checked by sound/soc/soc-topology.c."""

from schema import kernel_tokens as kt
from tplgparse import binary
from tplgparse.binary import DapmType, TplgType

from . import ERROR, WARNING, rule

SNDRV_CTL_ELEM_ACCESS_TLV_READ = 1 << 4
SNDRV_CTL_ELEM_ACCESS_TLV_WRITE = 1 << 5
SNDRV_CTL_ELEM_ACCESS_TLV_READWRITE = SNDRV_CTL_ELEM_ACCESS_TLV_READ | \
    SNDRV_CTL_ELEM_ACCESS_TLV_WRITE
SNDRV_CTL_ELEM_ACCESS_TLV_CALLBACK = 1 << 28
SNDRV_CTL_TLVT_DB_SCALE = 1
SND_SOC_TPLG_CTL_BYTES = 5

# soc_tplg_load_header(): types with a loader, everything else is vendor data
LOADED_TYPES = {TplgType.MIXER, TplgType.ENUM, TplgType.BYTES, TplgType.DAPM_GRAPH,
                TplgType.DAPM_WIDGET, TplgType.PCM, TplgType.DAI, TplgType.DAI_LINK,
                TplgType.BACKEND_LINK, TplgType.MANIFEST}


def at(offset):
    return f"offset 0x{offset:x}"


@rule("S001", "L1", ERROR, "soc_tplg_valid_header, soc_tplg_check_elem_count",
      "section headers and element counts are consistent with the file")
def binary_issues(ctx):
    for issue in ctx.tplg.raw.issues:
        sev = WARNING if issue.code == "payload-trailing" else ERROR
        yield at(issue.offset), issue.message, sev


@rule("S002", "L1", ERROR, "soc_tplg_vendor_load",
      "no section types without a kernel loader (SOF has no vendor_load)")
def vendor_sections(ctx):
    for sec in ctx.tplg.raw.sections:
        if sec.hdr.type not in LOADED_TYPES:
            yield at(sec.offset), f"{sec.type_name} section: no vendor load callback in SOF"


@rule("S003", "L1", ERROR, "soc_tplg_*_elems_load, soc_tplg_manifest_load",
      "element size fields match the kernel structure sizes")
def struct_sizes(ctx):
    t = ctx.tplg
    checks = []
    checks += [(w.label, w.raw.size, binary.SIZEOF["widget"]) for w in t.widgets]
    checks += [(c.label, c.hdr.size, binary.SIZEOF["ctl_hdr"]) for c in t.controls]
    checks += [(p.label, p.raw.size, binary.SIZEOF["pcm"]) for p in t.pcms]
    checks += [(link.label, link.raw.size, binary.SIZEOF["link"]) for link in t.links]
    checks += [(d.label, d.raw.size, binary.SIZEOF["dai"]) for d in t.dais]
    if t.manifest is not None:
        checks.append(("manifest", t.manifest.data.size, binary.SIZEOF["manifest"]))
    for where, size, expected in checks:
        if size != expected:
            yield where, f"structure size {size}, kernel expects {expected}"


@rule("S004", "L1", ERROR, "soc_tplg_check_name, soc_tplg_dapm_widget_create",
      "names are NUL terminated within 44 bytes")
def names_terminated(ctx):
    t = ctx.tplg
    fields = []
    for w in t.widgets:
        fields += [(w.label, "name", w.raw.name), (w.label, "stream name", w.raw.sname)]
    for c in t.controls:
        fields.append((c.label, "name", c.hdr.name))
        if c.kind == TplgType.ENUM:
            items = min(c.body["items"], binary.NUM_TEXTS)
            fields += [(c.label, f"text {i}", txt) for i, txt in enumerate(c.body.texts[:items])]
    for p in t.pcms:
        fields += [(p.label, "pcm name", p.raw.pcm_name), (p.label, "dai name", p.raw.dai_name)]
        fields += [(p.label, f"caps {i} name", cap.name) for i, cap in enumerate(p.raw.caps)]
    for link in t.links:
        fields += [(link.label, "name", link.raw.name),
                   (link.label, "stream name", link.raw.stream_name)]
    for r in t.routes:
        fields += [(r.label, "sink", r.raw.sink), (r.label, "source", r.raw.source),
                   (r.label, "control", r.raw.control)]
    for where, what, raw in fields:
        if not binary.name_terminated(raw):
            yield where, f"{what} is not NUL terminated"


@rule("S005", "L1", ERROR, "get_widget_id", "widget types are known to the kernel")
def widget_types(ctx):
    for w in ctx.tplg.widgets:
        if w.id not in DapmType.__members__.values():
            yield w.label, f"unknown widget type {w.id}"


@rule("S006", "L1", ERROR, "soc_tplg_denum_create",
      "enum controls have at most SND_SOC_TPLG_NUM_TEXTS items")
def enum_items(ctx):
    for c in ctx.tplg.controls:
        if c.kind == TplgType.ENUM and c.body["items"] > binary.NUM_TEXTS:
            yield c.label, f"{c.body['items']} items, maximum is {binary.NUM_TEXTS}"


@rule("S007", "L1", ERROR, "soc_tplg_create_tlv",
      "TLV controls without a TLV callback use a dB scale TLV")
def tlv_type(ctx):
    for c in ctx.tplg.controls:
        access = c.hdr.access
        if access & SNDRV_CTL_ELEM_ACCESS_TLV_READWRITE and \
           not access & SNDRV_CTL_ELEM_ACCESS_TLV_CALLBACK and \
           c.hdr.tlv.type != SNDRV_CTL_TLVT_DB_SCALE:
            yield c.label, f"unsupported TLV type {c.hdr.tlv.type}"


def _bind(ids, tables):
    "Return the set of handler names ('get', 'put', 'info') resolved for the IDs."
    found = set()
    for table in tables:
        for entry in table:
            for name, idx in (("get", 1), ("put", 2), ("info", 3)):
                if name not in found and len(entry) > idx and entry[idx] and \
                   entry[0] == ids[name]:
                    found.add(name)
    return found


@rule("S008", "L1", ERROR, "soc_tplg_kcontrol_bind_io",
      "kcontrol get/put/info handler IDs bind to kernel handlers")
def kcontrol_io(ctx):
    for c in ctx.tplg.controls:
        access = c.hdr.access
        ops = {n: c.hdr.ops[n] for n in ("get", "put", "info")}
        if c.kind == TplgType.BYTES and ops["info"] == SND_SOC_TPLG_CTL_BYTES and \
           access & SNDRV_CTL_ELEM_ACCESS_TLV_READWRITE and \
           access & SNDRV_CTL_ELEM_ACCESS_TLV_CALLBACK:
            ext = {"get": c.body.ext_ops["get"], "put": c.body.ext_ops["put"], "info": None}
            bound = _bind(ext, [kt.SOF_BYTES_EXT_OPS])
            if access & SNDRV_CTL_ELEM_ACCESS_TLV_READ and "get" not in bound:
                yield c.label, f"no bytes ext get handler for ID {ext['get']}"
            if access & SNDRV_CTL_ELEM_ACCESS_TLV_WRITE and "put" not in bound:
                yield c.label, f"no bytes ext put handler for ID {ext['put']}"
            continue
        bound = _bind(ops, [kt.SOF_IO_OPS, kt.STD_IO_OPS])
        missing = [f"{n} ID {ops[n]}" for n in ("get", "put", "info") if n not in bound]
        if missing:
            yield c.label, f"no kcontrol handler for {', '.join(missing)}"


@rule("S009", "L1", WARNING, "soc_tplg_manifest_load",
      "manifest element counts match the file contents")
def manifest_counts(ctx):
    t = ctx.tplg
    if t.manifest is None:
        yield "file", "no manifest section", ERROR
        return
    m = t.manifest.data
    actual = {
        "control": len(t.controls),
        "widget": len(t.widgets),
        "graph": len(t.routes),
        "pcm": len(t.pcms),
        "dai_link": len(t.links),
        "dai": len(t.dais),
    }
    for name, count in actual.items():
        declared = m[f"{name}_elems"]
        if declared != count:
            yield "manifest", f"declares {declared} {name} elements, file has {count}"
