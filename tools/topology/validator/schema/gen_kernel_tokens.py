#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2026 Intel Corporation. All rights reserved.

"""Generate kernel_tokens.py from a Linux kernel source tree.

The SOF driver describes which vendor tuples it consumes with static
tables of struct sof_topology_token. This script extracts those tables
so that the validator follows the kernel without hand-copied token
lists. Run it against a kernel tree when the SOF driver token handling
changes, or with --check in CI to detect drift.

    gen_kernel_tokens.py --kernel ~/linux             # rewrite kernel_tokens.py
    gen_kernel_tokens.py --kernel ~/linux --check     # exit 1 if out of date
"""

import argparse
import pathlib
import pprint
import re
import subprocess
import sys

OUTPUT = pathlib.Path(__file__).with_name("kernel_tokens.py")

TOKENS_H = "include/uapi/sound/sof/tokens.h"
SOF_AUDIO_H = "sound/soc/sof/sof-audio.h"
TOPOLOGY_C = "sound/soc/sof/topology.c"
IPC4_TOPOLOGY_C = "sound/soc/sof/ipc4-topology.c"
ASOC_H = "include/uapi/sound/asoc.h"
SOC_TOPOLOGY_C = "sound/soc/soc-topology.c"

# Tables in topology.c that are parsed for every IPC version
GENERIC_TABLES = (
    "dapm_widget_tokens",
    "comp_pin_tokens",
    "comp_input_pin_binding_tokens",
    "comp_output_pin_binding_tokens",
    "stream_tokens",
    "led_tokens",
    "common_dai_link_tokens",
)


def strip_comments(src: str) -> str:
    src = re.sub(r"/\*.*?\*/", " ", src, flags=re.S)
    return re.sub(r"//[^\n]*", " ", src)


def block(src: str, decl: str) -> str:
    "Return the brace enclosed initializer of a top level declaration."
    m = re.search(r"^" + decl + r"\s*=\s*\{", src, flags=re.M)
    if not m:
        raise ValueError(f"declaration not found: {decl}")
    depth, i = 1, m.end()
    while depth:
        depth += {"{": 1, "}": -1}.get(src[i], 0)
        i += 1
    return src[m.end():i - 1]


def parse_token_ids(src: str) -> dict:
    ids = {}
    for name, val in re.findall(r"^#define\s+(SOF_TKN_\w+)\s+(\w+)", src, flags=re.M):
        ids[name] = ids[val] if val in ids else int(val, 0)  # aliases like SOF_TKN_EFFECT_TYPE
    return ids


def parse_enum(src: str, name: str) -> list:
    body = re.search(r"enum " + name + r"\s*\{(.*?)\}", src, flags=re.S).group(1)
    return [m for m in re.findall(r"(\w+)\s*,", body) if m != "SOF_TOKEN_COUNT"]


def parse_token_tables(src: str) -> dict:
    "All top level 'static const struct sof_topology_token x[]' tables."
    tables = {}
    for name in re.findall(r"^static const struct sof_topology_token (\w+)\[\]", src, flags=re.M):
        body = block(src, r"static const struct sof_topology_token " + name + r"\[\]")
        tables[name] = [
            (tok, typ, getter) for tok, typ, getter in re.findall(
                r"\{\s*(SOF_TKN_\w+)\s*,\s*SND_SOC_TPLG_TUPLE_TYPE_(\w+)\s*,\s*(\w+)", body)]
    return tables


def parse_token_lists(src: str) -> dict:
    "All top level 'static enum sof_tokens x[]' widget token lists."
    lists = {}
    for name in re.findall(r"^static enum sof_tokens (\w+)\[\]", src, flags=re.M):
        body = block(src, r"static enum sof_tokens " + name + r"\[\]")
        lists[name] = re.findall(r"(SOF_\w+_TOKENS)", body)
    return lists


def parse_token_info(src: str, name: str) -> dict:
    "struct sof_token_info x[SOF_TOKEN_COUNT]: token group -> (description, table)"
    body = block(src, r"static const struct sof_token_info " + name + r"\[SOF_TOKEN_COUNT\]")
    return {grp: (desc, table) for grp, desc, table in re.findall(
        r"\[(SOF_\w+_TOKENS)\]\s*=\s*\{\s*\"([^\"]*)\"\s*,\s*(\w+)", body)}


def parse_widget_ops(src: str, name: str) -> dict:
    "struct sof_ipc_tplg_widget_ops x[]: dapm type -> (setup, token list)"
    body = block(src, r"static const struct sof_ipc_tplg_widget_ops " + name
                 + r"\[SND_SOC_DAPM_TYPE_COUNT\]")
    return {wtype: (setup, tlist if tlist != "NULL" else None) for wtype, setup, _free, tlist in
            re.findall(r"\[snd_soc_dapm_(\w+)\]\s*=\s*\{\s*(\w+)\s*,\s*(\w+)\s*,\s*(\w+)", body)}


def parse_dai_types(src: str) -> dict:
    body = block(src, r"static const struct sof_dai_types sof_dais\[\]")
    return dict(re.findall(r"\{\s*\"(\w+)\"\s*,\s*(SOF_DAI_\w+)\s*\}", body))


def parse_int_defines(src: str) -> dict:
    return {name: int(val, 0) for name, val in
            re.findall(r"^#define\s+(\w+)\s+(\d+|0x[0-9a-fA-F]+)\b", src, flags=re.M)}


def parse_ops(src: str, decl: str, defines: dict) -> list:
    """Kcontrol handler tables: [(id, has_get, has_put[, has_info])].

    Entries are {ID, get, put} or {ID, get, put, info}, missing ones are NULL.
    """
    out = []
    for entry in re.findall(r"\{([^{}]*)\}", block(src, decl)):
        fields = [f.strip() for f in entry.split(",") if f.strip()]
        out.append((defines[fields[0]],) + tuple(f != "NULL" for f in fields[1:]))
    return out


def kernel_version(kernel: pathlib.Path) -> str:
    try:
        return subprocess.run(["git", "-C", str(kernel), "describe", "--always", "--abbrev=12"],
                              capture_output=True, text=True, check=True).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return "unknown"


def generate(kernel: pathlib.Path) -> dict:
    def read(rel):
        return strip_comments((kernel / rel).read_text())

    tokens_h = read(TOKENS_H)
    token_ids = parse_token_ids(tokens_h)
    topology_c = read(TOPOLOGY_C)
    ipc4_c = read(IPC4_TOPOLOGY_C)
    defines = parse_int_defines(read(ASOC_H))
    defines.update(parse_int_defines(tokens_h))

    tables = parse_token_tables(topology_c)
    ipc4_tables = parse_token_tables(ipc4_c)
    ipc4_info = parse_token_info(ipc4_c, "ipc4_token_list")
    ipc4_lists = parse_token_lists(ipc4_c)
    ipc4_ops = parse_widget_ops(ipc4_c, "tplg_ipc4_widget_ops")

    def resolve(table):
        out = []
        for tok, typ, getter in table:
            if tok not in token_ids:
                raise ValueError(f"{tok} not defined in {TOKENS_H}")
            out.append((token_ids[tok], typ, getter))
        return out

    token_names = {}
    for name, tid in token_ids.items():
        token_names.setdefault(tid, name)  # first definition wins over aliases

    return {
        "KERNEL_VERSION": kernel_version(kernel),
        "TOKEN_NAMES": dict(sorted(token_names.items())),
        "TOKEN_GROUPS": parse_enum(read(SOF_AUDIO_H), "sof_tokens"),
        "GENERIC_TABLES": {name: resolve(tables[name]) for name in GENERIC_TABLES},
        "IPC4_TOKEN_LIST": {grp: (desc, resolve(ipc4_tables[table]))
                            for grp, (desc, table) in ipc4_info.items()},
        "IPC4_WIDGET_TOKEN_LISTS": {
            wtype: (setup, ipc4_lists[tlist] if tlist else [])
            for wtype, (setup, tlist) in ipc4_ops.items()},
        "DAI_TYPES": parse_dai_types(topology_c),
        # soc_tplg_kcontrol_bind_io(): standard and SOF kcontrol handlers by ID
        "STD_IO_OPS": parse_ops(read(SOC_TOPOLOGY_C),
                                r"static const struct snd_soc_tplg_kcontrol_ops io_ops\[\]",
                                defines),
        "SOF_IO_OPS": parse_ops(topology_c,
                                r"static const struct snd_soc_tplg_kcontrol_ops sof_io_ops\[\]",
                                defines),
        "SOF_BYTES_EXT_OPS": parse_ops(
            topology_c, r"static const struct snd_soc_tplg_bytes_ext_ops sof_bytes_ext_ops\[\]",
            defines),
    }


HEADER = '''\
# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2026 Intel Corporation. All rights reserved.
#
# GENERATED by gen_kernel_tokens.py from the Linux SOF driver, do not edit.
#
# Token tables are lists of (token id, SND_SOC_TPLG_TUPLE_TYPE_ name, getter)
# in the order the kernel declares them.
'''


def render(data: dict) -> str:
    out = [HEADER]
    for key, value in data.items():
        out.append(f"\n{key} = {pprint.pformat(value, width=100, sort_dicts=False)}\n")
    return "".join(out)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--kernel", required=True, type=pathlib.Path, help="Linux source tree")
    ap.add_argument("--check", action="store_true",
                    help="do not write, exit with 1 if the committed file is out of date")
    ap.add_argument("-o", "--output", type=pathlib.Path, default=OUTPUT)
    args = ap.parse_args()

    data = generate(args.kernel)

    if args.check:
        current = {}
        exec(args.output.read_text(), current)  # pylint: disable=exec-used
        stale = [k for k in data if k != "KERNEL_VERSION" and current.get(k) != data[k]]
        if stale:
            print(f"{args.output.name} is out of date with kernel {data['KERNEL_VERSION']}: "
                  f"{', '.join(stale)} differ; rerun without --check", file=sys.stderr)
            return 1
        print(f"{args.output.name} matches kernel {data['KERNEL_VERSION']}")
        return 0

    args.output.write_text(render(data))
    print(f"wrote {args.output} from kernel {data['KERNEL_VERSION']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
