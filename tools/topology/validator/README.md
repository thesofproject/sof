# Topology validator (`tools/topology/validator`)

`tplg_validate.py` checks SOF IPC4 topology binaries (`.tplg`) offline, without
target hardware. It parses the binary and repeats what the Linux SOF driver
does when it loads the topology, so that problems show up at build time
instead of on a device. `alsatplg` accepts many topologies that the kernel
rejects; for instance it prints `undefined sink widget` for a dangling route
and still exits successfully.

```bash
# check topologies, print errors and warnings
tplg_validate.py build_tools/topology/topology2/production/sof-mtl-rt711-4ch.tplg

# also print informational findings, check core IDs against a 5 core DSP
tplg_validate.py -v --num-cores 5 *.tplg

# list the rules and the kernel functions they mirror
tplg_validate.py --list-rules

# print the decoded topology with kernel token names
tplg_validate.py --dump foo.tplg

# check a feature topology on top of the topologies loaded before it, in load order
tplg_validate.py --base sof-sdca-1amp-id2.tplg --base sof-sdca-jack-id0.tplg \
    --base sof-sdca-jack-ref-dai.tplg sof-sdca-amp-ref.tplg
```

Exit status is 1 if any error is found (`--werror`: any warning). Python 3.8+
and the `construct` module (`python3-construct`) are required.

## Build integration

`add_alsatplg2_command` in `tools/topology/CMakeLists.txt` runs the validator
after `alsatplg` for every topology v2 target, unless configured with
`-DTPLG_VALIDATE=OFF` or `construct` is missing. A successful check writes a
`<topology>.tplg.validated` stamp file. An error fails the target and leaves no
stamp, so the check reruns on the next build. Changes to the validator sources
or to `known-issues.txt` recheck all topologies without rebuilding them.

Known errors are listed in `known-issues.txt` as `<topology glob> <rule>`
pairs and reported as warnings. Do not add entries to silence new errors:
fix the topology. Remove entries when a topology is fixed.

Feature topologies are listed there as well. The kernel loads them into the
same component after the function topologies and after the feature
topologies given before them, and their routes may refer to widgets of those
topologies. They are checked alone at build time, so check them by hand with
`--base`, giving the topologies loaded before them in load order.

## What is checked

Rules are grouped in layers by how closely they follow kernel code:

| Layer | Mirrors | Examples |
|-------|---------|----------|
| L1 | `sound/soc/soc-topology.c` | headers, element sizes, names, kcontrol handler IDs, TLV types |
| L2 | `sound/soc/sof/topology.c` tuple engine | kernel parse errors, tuple types, audio format blocks vs. counts |
| L3 | `topology.c`, `ipc4-topology.c` setup | routes, pipelines, chain DMA, DAI types, pins, bytes controls, PCM binding |
| L4 | `sof-audio.c` path walk, format selection | PCM formats vs. host copier formats (advisory) |

`F003` checks each firmware route's actual output/input pin pair. Single-pin
endpoints use pin 0; multi-pin endpoints require explicit bindings. Disjoint
declared format sets produce an error naming both pins and their formats, and
fail validation without `--werror`.
Comparison requires matching rate, channel count and valid-bit depth, with
sample types treated according to their declared representations. Signed
integer alignment variants share a format for 16/20/24 valid bits; at 32/32,
MSB, LSB and signed integer representations share the same full-width
two's-complement format. Other integer layouts and non-integer representations
remain distinct. `T008` also rejects unknown types, valid depths larger than
their containers, unsigned integer other than 8/8, float other than 32/32, and
A-law or mu-law other than 8/8. In SOF firmware, internal 24-bit MSB and LSB
integer declarations both use the same DSP format; copier gateway conversion
handles external alignment. The check validates declared format compatibility,
not the behavior of a particular kernel version or the sample alignment
performed at runtime. It does not validate container/layout compatibility
between pins or simulate runtime format selection: overlapping sets only
establish possible compatibility, not that every PCM configuration works.

Unresolved pins or missing formats are informational (`-v`), not guessed.
Mixer outputs share the module's output formats across pins. Undeclared
secondary copier outputs are configured from the sink at bind time;
they are not constrained to pin 0. Process output pins without declarations
use the pin-0 fallback. Missing process extension formats remain covered by
`W008`. Chain-DMA and DAPM-only routes are skipped. An unbound siggen input
has no firmware route to check. Feature routes use widgets from `--base`.

Every finding names the rule and the relevant kernel path it models. These
references explain how topology objects and connections are interpreted; they
do not make findings predictions about how a specific kernel version handles
an invalid or incompatible topology. For example:

```
foo.tplg: error: G001 route 'mixin.90.1' -> 'mixout.1.1': sink widget 'mixout.1.1' does not exist (kernel: snd_soc_dapm_add_routes, sof_route_load)
```

Severities: `error` means the topology fails validation, usually because the
kernel fails to load it or an affected PCM fails when used. `F003` also treats
disjoint declared pin formats as errors, without simulating firmware behavior.
`warning` means a likely mistake, or a failure that depends on runtime state the
validator cannot know. `info` lists observations (`-v`).

Not checked, because the data only exists on the target: firmware module
UUIDs and module types from the firmware manifest, ACPI NHLT blobs, machine
driver BE links, and the DSP's answers to IPC messages.

## Keeping up with the kernel

The kernel driver is the reference. The validator follows it in two ways:

- **Generated token schema.** `schema/kernel_tokens.py` is generated from a
  kernel tree by `schema/gen_kernel_tokens.py`. It holds the `SOF_TKN_*` IDs, the
  `struct sof_topology_token` tables with token types, the per widget type
  token lists of `tplg_ipc4_widget_ops`, the DAI type names and the kcontrol
  handler IDs. `tplgparse/kernel_engine.py` emulates the kernel tuple parser
  (`sof_parse_token_sets()`, `sof_copy_tuples()`, `sof_update_ipc_object()`)
  on top of these tables. New tokens or widget types only need a
  regeneration:

  ```bash
  schema/gen_kernel_tokens.py --kernel ~/linux           # update kernel_tokens.py
  schema/gen_kernel_tokens.py --kernel ~/linux --check   # CI: fail on drift
  ```

- **Hand written rules** in `checks/`. Each rule names the kernel function it
  mirrors (`--list-rules`). Changes that add or remove topology checks in
  `sound/soc/sof/ipc4-topology.c` or `topology.c` should get a matching rule
  update here.

`schema/check_tokens_conf.py` cross-checks `topology2/include/common/tokens.conf`
and the `token_ref` attributes of the topology2 classes against the kernel schema.

## Layout

```
tplg_validate.py        command line tool
tplgparse/binary.py     ALSA topology binary format (ported from sof-test tplgtool2.py)
tplgparse/model.py      typed view, vendor tuples decoded as written
tplgparse/kernel_engine.py  kernel tuple engine emulation
schema/                 generated kernel token schema and its generator
checks/                 rules, one module per layer
tests/                  pytest suite; tplgbuild.py builds test topologies
known-issues.txt        waived errors
```

Run the tests with `pytest tools/topology/validator/tests`. Set `ALSATPLG`
to select the `alsatplg` binary for the integration tests and `KERNEL_TREE`
to include the schema drift check.
