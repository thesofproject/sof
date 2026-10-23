# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2026 Intel Corporation. All rights reserved.

"""Integration tests against real topology2 sources and a kernel tree.

ALSATPLG selects the alsatplg binary (default: SOF tools/bin or PATH),
KERNEL_TREE a Linux tree for the token schema drift check.
"""

import os
import pathlib
import shutil
import subprocess
import sys

import pytest

import checks
from tplgparse import model

VALIDATOR = pathlib.Path(__file__).resolve().parents[1]
TOPOLOGY2 = VALIDATOR.parent / "topology2"
SOF_ROOT = VALIDATOR.parents[2]


def alsatplg():
    for cand in (os.environ.get("ALSATPLG"), SOF_ROOT.parent / "tools/bin/alsatplg",
                 shutil.which("alsatplg")):
        if cand and os.access(cand, os.X_OK):
            return str(cand)
    return None


def build_topology(tmp_path, conf, defines):
    tool = alsatplg()
    if tool is None:
        pytest.skip("alsatplg not found")
    abi = subprocess.run(["bash", str(TOPOLOGY2 / "get_abi.sh"), str(SOF_ROOT), "ipc4"],
                         check=True, capture_output=True).stdout
    src = tmp_path / "in.conf"
    src.write_bytes(abi + (TOPOLOGY2 / conf).read_bytes())
    out = tmp_path / "out.tplg"
    env = dict(os.environ, ALSA_CONFIG_DIR=str(TOPOLOGY2))
    subprocess.run([tool, "-I", str(TOPOLOGY2) + "/", "-D", defines, "-p", "-c", str(src),
                    "-o", str(out)], env=env, check=True, capture_output=True, cwd=tmp_path)
    return out


# defines of real targets in topology2/*/tplg-targets*.cmake
MTL_NOCODEC = ("PLATFORM=mtl,NUM_DMICS=4,PDM1_MIC_A_ENABLE=1,PDM1_MIC_B_ENABLE=1,"
               "PREPROCESS_PLUGINS=nhlt,NHLT_BIN=nhlt.bin")


@pytest.mark.parametrize("conf,defines", [
    ("cavs-nocodec.conf", MTL_NOCODEC),
    ("sof-hda-generic.conf", "HDA_CONFIG=mix,NUM_HDMIS=3"),
])
def test_real_topology_has_no_errors(tmp_path, conf, defines):
    out = build_topology(tmp_path, conf, defines)
    findings = checks.run(model.load(out), checks.Options())
    errors = [f.format() for f in findings if f.severity == checks.ERROR]
    assert errors == []


def test_cli_exit_status(tmp_path):
    out = build_topology(tmp_path, "cavs-nocodec.conf", MTL_NOCODEC)
    res = subprocess.run([sys.executable, str(VALIDATOR / "tplg_validate.py"), "-q", str(out)],
                         capture_output=True, text=True, check=False)
    assert res.returncode == 0, res.stdout + res.stderr


def test_kernel_schema_is_current():
    kernel = os.environ.get("KERNEL_TREE")
    if not kernel:
        pytest.skip("KERNEL_TREE not set")
    res = subprocess.run([sys.executable, str(VALIDATOR / "schema/gen_kernel_tokens.py"),
                          "--kernel", kernel, "--check"], capture_output=True, text=True,
                         check=False)
    assert res.returncode == 0, res.stderr
