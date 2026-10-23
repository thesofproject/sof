# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2026 Intel Corporation. All rights reserved.

"""L2: vendor tuples as read by the SOF driver tuple engine."""

from schema import kernel_tokens as kt
from tplgparse import kernel_engine as ke
from tplgparse.binary import TplgType, TupleType

from . import ERROR, INFO, WARNING, rule

TKN_IN_RATE = 1900
TKN_OUT_RATE = 1930

FORMAT_GROUPS = (
    ("input", "SOF_IN_AUDIO_FORMAT_TOKENS", TKN_IN_RATE),
    ("output", "SOF_OUT_AUDIO_FORMAT_TOKENS", TKN_OUT_RATE),
)

# rate, bit depth, channel map, FMT_CFG and pin index tokens per direction,
# for readable messages
FORMAT_FIELDS = {
    TKN_IN_RATE: (1900, 1901, 1904, 1907, 1909),
    TKN_OUT_RATE: (1930, 1931, 1934, 1937, 1939),
}
SAMPLE_TYPES = {0: " MSB-integer", 1: "", 2: " signed", 3: " unsigned", 4: " float",
                5: " A-law", 6: " mu-law"}


def tname(token: int) -> str:
    return kt.TOKEN_NAMES.get(token, f"token {token}")


def category(tuple_type: str) -> str:
    "Element layout the kernel uses for a token type."
    return {"STRING": "string", "UUID": "uuid"}.get(tuple_type, "value")


def array_category(ttype: TupleType) -> str:
    return category(ttype.name)


def consumed_tables(ctx, widget):
    "All token tables the kernel applies to this widget's private data."
    gt = kt.GENERIC_TABLES
    tables = [gt["dapm_widget_tokens"], gt["comp_pin_tokens"],
              gt["comp_input_pin_binding_tokens"], gt["comp_output_pin_binding_tokens"]]
    tables += [ke.group(g) for g in ctx.kw(widget).token_groups]
    return tables


def kernel_types(tables) -> dict:
    "token -> set of element layouts the kernel reads it with"
    types = {}
    for table in tables:
        for tok, typ, _getter in table:
            types.setdefault(tok, set()).add(category(typ))
    return types


def tuple_objects(ctx):
    """(object, private data, token tables) for all tuple carrying objects."""
    t = ctx.tplg
    gt = kt.GENERIC_TABLES
    for w in t.widgets:
        yield w, w.priv, consumed_tables(ctx, w)
    for p in t.pcms:
        yield p, p.priv, [gt["stream_tokens"]]
    for link in t.links:
        yield link, link.priv, [gt["common_dai_link_tokens"]]
    for c in t.controls:
        if c.kind == TplgType.MIXER and c.priv.data:
            yield c, c.priv, [gt["led_tokens"]]


@rule("T001", "L2", ERROR, "sof_widget_ready, sof_widget_parse_tokens",
      "widget tuples parse without kernel errors")
def widget_parse_errors(ctx):
    for w in ctx.tplg.widgets:
        err = ctx.kw(w).error
        if err:
            yield w.label, f"{err} (in {err.kernel_ref})"


@rule("T002", "L2", ERROR, "sof_copy_tuples, sof_parse_token_sets",
      "the kernel reads every value from the tuple it was meant to come from")
def misreads(ctx):
    for w in ctx.tplg.widgets:
        genuine = {(t.offset, t.token, array_category(t.type)) for t in w.priv.tuples}
        seen = set()
        for r in ctx.kw(w).trace.reads:
            if (r.offset, r.token, r.kind) in genuine or (r.offset, r.token) in seen:
                continue
            seen.add((r.offset, r.token))
            yield w.label, (f"kernel reads {tname(r.token)} = {r.value!r} from private data "
                            f"offset {r.offset}, which is not such a tuple in the topology")


@rule("T003", "L2", ERROR, "sof_parse_token_sets",
      "vendor arrays are well formed")
def array_issues(ctx):
    # mixer controls are included: sof_control_load_volume() parses LED tokens
    for obj, priv, _tables in tuple_objects(ctx):
        for issue in priv.issues:
            yield obj.label, f"private data offset {issue.offset}: {issue.message}"


@rule("T004", "L2", WARNING, "include/uapi/sound/sof/tokens.h",
      "all tuple tokens are known to the kernel")
def unknown_tokens(ctx):
    for obj, priv, _tables in tuple_objects(ctx):
        unknown = sorted({t.token for t in priv.tuples if t.token not in kt.TOKEN_NAMES})
        if unknown:
            yield obj.label, f"tokens unknown to the kernel: {', '.join(map(str, unknown))}"


@rule("T005", "L2", ERROR, "sof_parse_token_sets, sof_copy_tuples",
      "tuple types match the token types the kernel expects")
def type_mismatch(ctx):
    for obj, priv, tables in tuple_objects(ctx):
        expected = kernel_types(tables)
        for t in priv.tuples:
            want = expected.get(t.token)
            if want and array_category(t.type) not in want:
                yield obj.label, (f"{tname(t.token)} is in a {t.type.name} tuple array, the "
                                  f"kernel reads it as {'/'.join(sorted(want))} and will "
                                  f"not see this value")


def format_blocks(priv, tokens) -> list:
    """Split the tuples of one direction into audio format blocks.

    Vendor array boundaries are not format boundaries: alsatplg emits one
    array per format, but an array may hold several. A block ends where one
    of its tokens repeats, which is also how the kernel assigns the n-th
    occurrence of each token to format n.
    """
    blocks, cur = [], {}
    for t in priv.tuples:
        if t.token not in tokens:
            continue
        if t.token in cur:
            blocks.append(cur)
            cur = {}
        cur[t.token] = t.value
    if cur:
        blocks.append(cur)
    return blocks


def describe(block: dict, rate_token: int, detail=False) -> str:
    "Format summary from the values the kernel uses, valid bits etc. from FMT_CFG."
    rate, depth, ch_map, cfg, pin = (block.get(t, 0) for t in FORMAT_FIELDS[rate_token])
    stype = (cfg >> 16) & 0xff
    text = (f"{rate}Hz {depth}/{(cfg >> 8) & 0xff}bit"
            f"{SAMPLE_TYPES.get(stype, f' type {stype}')} {cfg & 0xff}ch")
    if detail:
        text += f" fmt_cfg 0x{cfg:x} ch_map 0x{ch_map:x}" + (f" pin {pin}" if pin else "")
    return text


@rule("T006", "L2", WARNING, "sof_widget_parse_tokens, sof_ipc4_get_audio_fmt",
      "number of audio format blocks matches NUM_INPUT/OUTPUT_AUDIO_FORMATS")
def format_counts(ctx):
    for w in ctx.tplg.widgets:
        kw = ctx.kw(w)
        if kw.error:
            continue
        n_in, n_out, ins, outs = kw.audio_formats()
        for (direction, grp, rate_token), num, sets in zip(FORMAT_GROUPS, (n_in, n_out),
                                                           (ins, outs)):
            if grp not in kw.token_groups:
                continue
            group_tokens = [tok for tok, _typ, _g in ke.group(grp)]
            blocks = format_blocks(w.priv, set(group_tokens))
            if len(blocks) < num:
                yield w.label, (f"declares {num} {direction} formats but defines "
                                f"{len(blocks)}"), ERROR
                continue
            # the kernel fills format n from the n-th occurrence of each token,
            # fields without a value stay 0 from kzalloc()
            for i, (kset, block) in enumerate(zip(sets, blocks)):
                diff = [tok for tok in group_tokens if kset.get(tok, 0) != block.get(tok, 0)]
                if diff:
                    yield w.label, (f"{direction} format {i} as read by the kernel differs "
                                    f"from the topology in {', '.join(map(tname, diff))}; "
                                    f"a format block is missing tokens"), ERROR
                    break
            if len(blocks) <= num:
                continue

            # compare what the kernel would use, not the descriptions
            def key(block):
                return tuple(block.get(tok, 0) for tok in group_tokens)
            used = {key(b) for b in blocks[:num]}
            new = list({key(b): b for b in blocks[num:] if key(b) not in used}.values())
            if not new:
                yield w.label, (f"defines {len(blocks)} {direction} formats but declares "
                                f"{num}; the extra ones duplicate used formats"), INFO
                continue
            short = [describe(b, rate_token) for b in blocks[:num] + new]
            detail = len(set(short)) < len(short)  # distinct formats, same summary
            yield w.label, (f"defines {len(blocks)} {direction} formats but declares {num}; "
                            f"the kernel ignores "
                            f"{', '.join(describe(b, rate_token, detail) for b in new)}")


@rule("T007", "L2", INFO, "tplg_ipc4_widget_ops token lists",
      "tokens that the IPC4 kernel driver does not use for this widget type")
def unused_tokens(ctx):
    for w in ctx.tplg.widgets:
        used = kernel_types(consumed_tables(ctx, w))
        unused = sorted({t.token for t in w.priv.tuples
                         if t.token in kt.TOKEN_NAMES and t.token not in used})
        if unused:
            yield w.label, f"{w.type_name}: {', '.join(map(tname, unused))}"
