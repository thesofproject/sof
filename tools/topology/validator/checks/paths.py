# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2026 Intel Corporation. All rights reserved.

"""L4: PCM path and audio format simulation (advisory)."""

from dataclasses import dataclass, field

from . import INFO, WARNING, rule

PLAYBACK, CAPTURE = 0, 1
DIR_NAME = ("playback", "capture")
HOST_TYPE = ("aif_in", "aif_out")
DAI_TYPE = ("dai_in", "dai_out")


@dataclass
class PcmPath:
    "Widgets and routes the kernel walks for one PCM direction."
    pcm: object
    direction: int
    host: object
    widgets: list = field(default_factory=list)  # in walk order, host first
    routes: list = field(default_factory=list)  # [(route, src, sink)]


def pcm_paths(ctx) -> list:
    """Mirror snd_soc_dapm_dai_get_connected_widgets() with dpcm_end_walk_at_be
    as used by sof_widget_list_setup(): starting from the host copier bound
    by spcm_bind(), follow routes downstream for playback and upstream for
    capture, stopping at DAI widgets (the BE end)."""
    cache = getattr(ctx, "_pcm_paths", None)
    if cache is not None:
        return cache

    t = ctx.tplg
    down, up = {}, {}
    for r in t.routes:
        src, sink = t.widget(r.source), t.widget(r.sink)
        if src and sink:
            down.setdefault(src.name, []).append((r, src, sink))
            up.setdefault(sink.name, []).append((r, src, sink))

    paths = []
    for p in t.pcms:
        for d, enabled in ((PLAYBACK, p.raw.playback), (CAPTURE, p.raw.capture)):
            if not enabled:
                continue
            name = p.raw.caps[d].name.split(b"\0", 1)[0].decode("ascii", "replace")
            host = next((w for w in t.widgets
                         if w.type_name == HOST_TYPE[d] and w.sname == name), None)
            if host is None:
                continue  # reported by P001
            path = PcmPath(p, d, host, [host])
            seen = {host.name}
            todo = [host]
            while todo:
                w = todo.pop(0)
                if w is not host and w.type_name in DAI_TYPE:
                    continue
                for r, src, sink in (down if d == PLAYBACK else up).get(w.name, ()):
                    path.routes.append((r, src, sink))
                    nxt = sink if d == PLAYBACK else src
                    if nxt.name not in seen:
                        seen.add(nxt.name)
                        path.widgets.append(nxt)
                        todo.append(nxt)
            paths.append(path)

    ctx._pcm_paths = paths
    return paths


def routes_on_pcm_paths(ctx) -> dict:
    "id(route) -> sorted names of the PCMs whose path includes the route"
    out = {}
    for p in pcm_paths(ctx):
        for r, _s, _k in p.routes:
            out.setdefault(id(r), set()).add(f"'{p.pcm.name}' {DIR_NAME[p.direction]}")
    return {k: sorted(v) for k, v in out.items()}


# --- format simulation -------------------------------------------------------

# SNDRV_PCM_RATE_* bit -> rate (include/sound/pcm.h)
PCM_RATES = {0: 5512, 1: 8000, 2: 11025, 3: 16000, 4: 22050, 5: 32000, 6: 44100, 7: 48000,
             8: 64000, 9: 88200, 10: 96000, 11: 176400, 12: 192000, 13: 352800, 14: 384000,
             15: 705600, 16: 768000, 17: 12000, 18: 24000, 19: 128000}

# SOF_IPC4_TYPE_* (sound/soc/sof/ipc4-topology.h)
TYPE_LSB_INTEGER, TYPE_UNSIGNED_INTEGER, TYPE_FLOAT, TYPE_A_LAW, TYPE_MU_LAW = 1, 3, 4, 5, 6

# SNDRV_PCM_FMTBIT_* bit -> (name, valid bits, IPC4 type), per
# sof_ipc4_get_valid_bits() and sof_ipc4_get_sample_type()
PCM_FORMATS = {
    1: ("U8", 8, TYPE_UNSIGNED_INTEGER),
    2: ("S16_LE", 16, TYPE_LSB_INTEGER),
    6: ("S24_LE", 24, TYPE_LSB_INTEGER),
    10: ("S32_LE", 32, TYPE_LSB_INTEGER),
    14: ("FLOAT_LE", 32, TYPE_FLOAT),
    20: ("MU_LAW", 8, TYPE_MU_LAW),
    21: ("A_LAW", 8, TYPE_A_LAW),
}

TKN_IN_RATE, TKN_IN_FMT_CFG, TKN_IN_PIN = 1900, 1907, 1909
TKN_OUT_RATE, TKN_OUT_FMT_CFG, TKN_OUT_PIN = 1930, 1937, 1939


def fmt_key(fmt: dict, rate_tok, cfg_tok):
    """(rate, channels, valid bits, type) as compared by
    sof_ipc4_init_input_audio_fmt(), using sof_ipc4_fmt_cfg_to_type()."""
    cfg = fmt.get(cfg_tok, 0)
    bits = (cfg >> 8) & 0xff
    stype = (cfg >> 16) & 0xff if bits in (8, 32) else TYPE_LSB_INTEGER
    return fmt.get(rate_tok, 0), cfg & 0xff, bits, stype


def caps_params(caps):
    "All (rate, channels, format name, valid bits, type) a PCM caps entry allows."
    rates = [r for bit, r in PCM_RATES.items() if caps.rates & (1 << bit)]
    if caps.rate_min or caps.rate_max:
        rates = [r for r in rates if caps.rate_min <= r <= (caps.rate_max or r)]
    formats = [f for bit, f in PCM_FORMATS.items() if caps.formats & (1 << bit)]
    for ch in range(caps.channels_min, caps.channels_max + 1):
        for rate in rates:
            for name, bits, stype in formats:
                yield rate, ch, name, bits, stype


@rule("F001", "L4", INFO, "sof_ipc4_init_input_audio_fmt, sof_pcm_open",
      "every rate/format/channel combination a PCM allows matches a host copier format "
      "(advisory: DPCM may narrow rates and channels further)")
def pcm_host_formats(ctx):
    for path in pcm_paths(ctx):
        kw = ctx.kw(path.host)
        if kw.error:
            continue
        _ni, _no, ins, outs = kw.audio_formats()
        if path.direction == PLAYBACK:
            fmts, keys = ins, (TKN_IN_RATE, TKN_IN_FMT_CFG, TKN_IN_PIN)
        else:
            fmts, keys = outs, (TKN_OUT_RATE, TKN_OUT_FMT_CFG, TKN_OUT_PIN)
        if not fmts:
            continue
        avail = [fmt_key(f, keys[0], keys[1]) for f in fmts if not f.get(keys[2], 0)]
        if len({a[:3] for a in avail}) <= 1:
            continue  # single format: the kernel uses it without matching
        bad = [(rate, ch, name) for rate, ch, name, bits, stype in
               caps_params(path.pcm.raw.caps[path.direction])
               if (rate, ch, bits, stype) not in avail]
        if bad:
            shown = ", ".join(f"{r}Hz {n} {c}ch" for r, c, n in bad[:4])
            more = f" and {len(bad) - 4} more" if len(bad) > 4 else ""
            yield path.pcm.label, (f"{DIR_NAME[path.direction]}: host copier "
                                   f"'{path.host.name}' has no format for {shown}{more}; "
                                   f"hw_params fails with 'Unsupported audio format'")


@rule("F002", "L4", WARNING, "sof_ipc4_init_input_audio_fmt, sof_pcm_open",
      "every sample format a PCM allows has a host copier format")
def pcm_host_sample_formats(ctx):
    for path in pcm_paths(ctx):
        kw = ctx.kw(path.host)
        if kw.error:
            continue
        _ni, _no, ins, outs = kw.audio_formats()
        if path.direction == PLAYBACK:
            fmts, keys = ins, (TKN_IN_RATE, TKN_IN_FMT_CFG, TKN_IN_PIN)
        else:
            fmts, keys = outs, (TKN_OUT_RATE, TKN_OUT_FMT_CFG, TKN_OUT_PIN)
        avail = [fmt_key(f, keys[0], keys[1]) for f in fmts if not f.get(keys[2], 0)]
        if len({a[:3] for a in avail}) <= 1:
            continue
        have = {(bits, stype) for _r, _c, bits, stype in avail}
        caps = path.pcm.raw.caps[path.direction]
        missing = [name for bit, (name, bits, stype) in PCM_FORMATS.items()
                   if caps.formats & (1 << bit) and (bits, stype) not in have]
        if missing:
            yield path.pcm.label, (f"{DIR_NAME[path.direction]}: PCM allows "
                                   f"{', '.join(missing)} but host copier "
                                   f"'{path.host.name}' has no such format")
