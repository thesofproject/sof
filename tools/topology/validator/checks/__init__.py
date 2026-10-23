# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2026 Intel Corporation. All rights reserved.

"""Rule registry and runner.

A rule is a generator function taking a Context and yielding
(where, message) or (where, message, severity) tuples. Each rule names
the kernel function it mirrors so that it can be reviewed against kernel
changes, see `tplg_validate.py --list-rules`.

Layers:
  L1  binary structure, generic ASoC loader (soc-topology.c)
  L2  vendor tuples as read by the SOF tuple engine (topology.c)
  L3  IPC4 load time and pipeline setup rules (topology.c, ipc4-topology.c)
  L4  PCM path and audio format simulation (sof-audio.c, ipc4-topology.c)
"""

import importlib
from dataclasses import dataclass, field

from tplgparse import kernel_engine

ERROR, WARNING, INFO = "error", "warning", "info"
SEVERITIES = (ERROR, WARNING, INFO)


@dataclass
class Rule:
    id: str
    layer: str
    severity: str
    kernel: str
    summary: str
    func: object


@dataclass
class Finding:
    rule: str
    severity: str
    where: str
    message: str
    kernel: str

    def format(self) -> str:
        return f"{self.severity}: {self.rule} {self.where}: {self.message} (kernel: {self.kernel})"


@dataclass
class Options:
    num_cores: int = 0  # 0: unknown, do not check
    verbose: bool = False


@dataclass
class Context:
    tplg: object
    options: Options
    _kernel_widgets: dict = field(default_factory=dict)

    def kw(self, widget) -> kernel_engine.KernelWidget:
        "Kernel view of a widget, computed once."
        key = id(widget)
        if key not in self._kernel_widgets:
            self._kernel_widgets[key] = kernel_engine.widget_ready(widget)
        return self._kernel_widgets[key]

    def pipeline_widget(self, widget):
        """The scheduler widget sof_complete() binds the widget to.

        Widgets are matched by section index, swidget->pipeline_id.
        """
        for w in self.tplg.widgets + self.tplg.base_widgets:
            if w.type_name == "scheduler" and w.index == widget.index:
                return w
        return None


RULES = []

MODULES = ("structure", "tokens", "ipc4", "paths")


def rule(rule_id, layer, severity, kernel, summary):
    "Register a rule function."
    def wrap(func):
        RULES.append(Rule(rule_id, layer, severity, kernel, summary, func))
        return func
    return wrap


def load_rules():
    for mod in MODULES:
        importlib.import_module(f"checks.{mod}")
    return RULES


def run(tplg, options: Options, selected=None) -> list:
    """Run all (or the selected) rules over a topology.

    A rule only runs when every finding of the layers below it is clean
    enough for its input to make sense: L2+ is skipped if L1 found errors,
    since the parsed model may be incomplete.
    """
    ctx = Context(tplg, options)
    findings = []
    l1_errors = False
    for r in load_rules():
        if selected is not None and r.id not in selected:
            continue
        if r.layer != "L1" and l1_errors:
            continue
        try:
            for item in r.func(ctx) or ():
                where, message = item[0], item[1]
                sev = item[2] if len(item) > 2 else r.severity
                findings.append(Finding(r.id, sev, where, message, r.kernel))
                if r.layer == "L1" and sev == ERROR:
                    l1_errors = True
        except Exception as e:  # pylint: disable=broad-except
            # malformed input must fail this file, not stop the validator
            findings.append(Finding(r.id, ERROR, "file",
                                    f"validator internal error: {type(e).__name__}: {e}",
                                    r.kernel))
    return findings
