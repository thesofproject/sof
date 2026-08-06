#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2026 Intel Corporation. All rights reserved.
"""
sof-shell-validate.py - Validate SOF Zephyr shell commands on the spider DUT.

Deploys the shell-enabled firmware to the NFS rootfs, triggers a firmware
reload on the DUT, then exercises every 'sof' shell command documented in
SHELL.md via cavstool.py's --shell-pty mechanism.  Prints a PASS/FAIL summary.

Usage
-----
  ./sof/scripts/sof-shell-validate.py [options]

Options
-------
  --build-dir DIR   Shell-enabled build directory (default: build-tgl-shell-llvm)
  --dut HOST        DUT SSH hostname (default: spider)
  --nfs-root PATH   NFS rootfs on the host (default: /srv/nfs/spider-rootfs)
  --no-deploy       Skip firmware copy step
  --no-reload       Skip rmmod/modprobe step
  --cavstool PATH   cavstool.py path on the DUT (default: auto-detected)
  --timeout SEC     Per-command reply timeout in seconds (default: 4)
"""

import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile
import textwrap
import time

# ---------------------------------------------------------------------------
# Command table: (shell_command, [strings that must appear OR-wise in output])
# An empty list means any non-error output is acceptable.
# ---------------------------------------------------------------------------
COMMANDS = [
    # Basic info
    ("sof version",           ["SOF Version"]),
    # Heap
    ("sof module heap",       ["No components found", "comp id"]),
    # Pipeline / module introspection
    # pipeline list always prints "ID  Core  Status  Priority  Period" header
    ("sof pipeline list",     ["ID"]),
    ("sof pipeline",          ["No pipelines found", "ppl_id"]),
    ("sof pipeline latency",  ["No active pipelines", "ppl_id"]),
    ("sof module",            ["No components found", "comp_id"]),
    ("sof stream",            ["ppl_id", "No active audio streams"]),
    # Hardware / platform
    ("sof core",              ["core", "enabled"]),
    # IPC
    ("sof ipc stats",         ["rx_count", "tx_count"]),
    ("sof ipc last",          []),          # ok even when empty (no IPC yet)
    # Buffers / scheduler
    ("sof buffer list",       ["Audio buffers"]),
    ("sof sched tasks",       ["Active scheduler"]),
    ("sof sched load",        ["Scheduler task cycle"]),
    # Logging
    ("sof log",               ["Log backends"]),
    # Controls
    ("sof kctl list",         ["No components found", "comp_id"]),
    # DAI and DMA
    ("sof dai list",          ["DAI", "dai", "SSP", "HDA", "DMIC", "ALH", "UAOL", "SAI", "ESAI"]),
    ("sof dma",               ["No DMA controllers", "DMA controller"]),
    # Verbose/multi-line commands last
    ("sof module list",       ["Module", "module", "UUID"]),
    ("sof mailbox hex",       ["Mailbox regions", "exception"]),
    ("sof dbgwin dump",       ["ADSP debug window"]),
]

# Patterns that indicate a command went wrong regardless of output
ERROR_PATTERNS = [
    "shell: command not found",
    "shell: unknown command",
    "-EINVAL",
    "-ENOMEM",
    "Traceback",
]

FIRMWARE_RELPATH = "zephyr/zephyr.ri"
FIRMWARE_NFS_SUBPATH = "lib/firmware/intel/sof-ipc4/tgl/community/sof-tgl.ri"
SHELL_PROMPT = "~$ "

# ---------------------------------------------------------------------------
# On-DUT Python snippet template.
# Substitution uses {name} placeholders; literal braces are doubled.
# Triple-quoted strings inside the snippet use single quotes to avoid
# conflicting with the outer r""" delimiter.
# Sent via SSH stdin; prints a single JSON list to stdout.
# ---------------------------------------------------------------------------
_DUT_SNIPPET_TMPL = (
    "import os, sys, select, json, time, subprocess, termios, atexit\n"
    "\n"
    "TIMEOUT  = {timeout}\n"
    "PROMPT   = '~$ '\n"
    "CAVSTOOL = {cavstool!r}\n"
    "COMMANDS = {commands!r}\n"
    "ERROR_PATTERNS = {error_patterns!r}\n"
    "\n"
    "def find_cavstool():\n"
    "    import shutil\n"
    "    for p in [CAVSTOOL,\n"
    "              '/usr/bin/cavstool.py',\n"
    "              '/usr/sbin/cavstool.py',\n"
    "              '/usr/local/bin/cavstool.py']:\n"
    "        if p and os.path.isfile(p):\n"
    "            return p\n"
    "    return shutil.which('cavstool.py')\n"
    "\n"
    "cavstool_path = find_cavstool()\n"
    "if not cavstool_path:\n"
    "    print(json.dumps({{'error': 'cavstool.py not found on DUT'}}))\n"
    "    sys.exit(1)\n"
    "\n"
    "# Wake the SOF audio PCI device from runtime PM before attaching cavstool.\n"
    "# The device suspends when idle; cavstool reads 0xffffffff from MMIO otherwise.\n"
    "import glob\n"
    "_sof_pci = glob.glob('/sys/bus/pci/devices/*/driver')\n"
    "_sof_pci = [os.path.dirname(d) for d in _sof_pci\n"
    "            if 'sof-audio' in os.path.basename(os.readlink(d))]\n"
    "for _dev in _sof_pci:\n"
    "    _ctrl = os.path.join(_dev, 'power', 'control')\n"
    "    if os.path.exists(_ctrl):\n"
    "        with open(_ctrl, 'w') as _f:\n"
    "            _f.write('on\\n')\n"
    "time.sleep(1)  # allow device to resume\n"
    "\n"
    "proc = subprocess.Popen(\n"
    "    [sys.executable, cavstool_path, '--log-only', '--shell-pty', '--no-history'],\n"
    "    stdout=subprocess.PIPE, stderr=subprocess.STDOUT,\n"
    "    text=True, bufsize=1,\n"
    ")\n"
    "atexit.register(proc.terminate)\n"
    "\n"
    "pty_path = None\n"
    "deadline = time.time() + 15\n"
    "while time.time() < deadline:\n"
    "    line = proc.stdout.readline()\n"
    "    if not line:\n"
    "        break\n"
    "    if 'shell PTY at:' in line:\n"
    "        pty_path = line.split('shell PTY at:')[-1].strip()\n"
    "        break\n"
    "\n"
    "if not pty_path:\n"
    "    print(json.dumps({{'error': 'cavstool did not report a shell PTY'}}))\n"
    "    proc.terminate()\n"
    "    sys.exit(1)\n"
    "\n"
    "fd = os.open(pty_path, os.O_RDWR | os.O_NOCTTY)\n"
    "old_attrs = termios.tcgetattr(fd)\n"
    "new_attrs = list(old_attrs)\n"
    "new_attrs[3] &= ~(termios.ECHO | termios.ICANON | termios.ISIG)\n"
    "new_attrs[6][termios.VMIN]  = 0\n"
    "new_attrs[6][termios.VTIME] = 0\n"
    "termios.tcsetattr(fd, termios.TCSANOW, new_attrs)\n"
    "atexit.register(lambda: termios.tcsetattr(fd, termios.TCSANOW, old_attrs))\n"
    "\n"
    "def drain(fd, secs):\n"
    "    # Read all available bytes within secs seconds.\n"
    "    buf = b''\n"
    "    deadline = time.time() + secs\n"
    "    while True:\n"
    "        remaining = deadline - time.time()\n"
    "        if remaining <= 0:\n"
    "            break\n"
    "        ready, _, _ = select.select([fd], [], [], min(0.1, remaining))\n"
    "        if not ready:\n"
    "            continue\n"
    "        chunk = os.read(fd, 4096)\n"
    "        if not chunk:\n"
    "            break\n"
    "        buf += chunk\n"
    "    return buf.decode('utf-8', errors='replace')\n"
    "\n"
    "drain(fd, 3)  # wait for first prompt\n"
    "\n"
    "results = []\n"
    "for (cmd, patterns) in COMMANDS:\n"
    "    drain(fd, 0.3)\n"
    "    os.write(fd, (cmd + '\\r\\n').encode())\n"
    "    collected = ''\n"
    "    deadline = time.time() + TIMEOUT\n"
    "    while time.time() < deadline:\n"
    "        chunk = drain(fd, 0.3)\n"
    "        collected += chunk\n"
    "        if PROMPT in collected:\n"
    "            break\n"
    "    output = collected\n"
    "    for s in [cmd, PROMPT, '\\r\\n', '\\r', '\\n\\n']:\n"
    "        output = output.replace(s, '\\n').strip()\n"
    "    has_error = any(ep in collected for ep in ERROR_PATTERNS)\n"
    "    has_match = (not patterns) or any(\n"
    "        p.lower() in collected.lower() for p in patterns\n"
    "    )\n"
    "    results.append({{'cmd': cmd, 'output': output,\n"
    "                    'pass': has_match and not has_error}})\n"
    "\n"
    "print(json.dumps(results))\n"
    "proc.terminate()\n"
    "# Restore runtime PM\n"
    "for _dev in _sof_pci:\n"
    "    _ctrl = os.path.join(_dev, 'power', 'control')\n"
    "    if os.path.exists(_ctrl):\n"
    "        with open(_ctrl, 'w') as _f:\n"
    "            _f.write('auto\\n')\n"
)

# ---------------------------------------------------------------------------
# Host-side helpers
# ---------------------------------------------------------------------------

def deploy_firmware(build_dir: str, nfs_root: str) -> None:
    src = os.path.join(build_dir, FIRMWARE_RELPATH)
    dst = os.path.join(nfs_root, FIRMWARE_NFS_SUBPATH)
    if not os.path.isfile(src):
        sys.exit(f"[error] Firmware not found: {src}")
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    print(f"[deploy] {src} -> {dst}")
    shutil.copy2(src, dst)


def reload_firmware(dut: str) -> None:
    print(f"[reload] Reloading SOF firmware on {dut} ...")
    # Try a soft reload first (non-destructive)
    soft = (
        "for f in /sys/bus/platform/devices/*/firmware_reload "
        "         /sys/kernel/debug/sof/reload_fw; do "
        "  [ -f \"$f\" ] && echo 1 > \"$f\" && exit 0; "
        "done; "
        # Fallback: rmmod + modprobe
        "rmmod snd_sof_pci_intel_tgl 2>/dev/null; "
        "modprobe snd_sof_pci_intel_tgl"
    )
    result = subprocess.run(
        ["ssh", "-o", "StrictHostKeyChecking=no",
         "-o", "PasswordAuthentication=no",
         f"root@{dut}", soft],
        capture_output=True, text=True, timeout=30,
    )
    if result.returncode != 0:
        print(f"[reload] Warning: reload command returned {result.returncode}")
        print(result.stderr.strip())
    else:
        print("[reload] OK — waiting 4 s for DSP to boot ...")
        time.sleep(4)


def find_cavstool_on_dut(dut: str) -> str:
    """Return the first cavstool.py path found on the DUT, or empty string."""
    result = subprocess.run(
        ["ssh", "-o", "StrictHostKeyChecking=no",
         "-o", "PasswordAuthentication=no",
         f"root@{dut}",
         "for p in /usr/bin/cavstool.py /usr/sbin/cavstool.py "
         "         /usr/local/bin/cavstool.py; do "
         "  [ -f \"$p\" ] && echo \"$p\" && exit 0; done; "
         "command -v cavstool.py 2>/dev/null || true"],
        capture_output=True, text=True, timeout=10,
    )
    return result.stdout.strip()


def run_on_dut(dut: str, snippet: str) -> str:
    """Execute a Python snippet on the DUT via SSH stdin piping."""
    result = subprocess.run(
        ["ssh", "-o", "StrictHostKeyChecking=no",
         "-o", "PasswordAuthentication=no",
         "-o", "ConnectTimeout=10",
         f"root@{dut}", "python3"],
        input=snippet.encode(),
        capture_output=True,
        timeout=180,
    )
    return result.stdout.decode(), result.stderr.decode()


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> None:
    ap = argparse.ArgumentParser(
        description="Validate SOF Zephyr shell commands on the spider DUT.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    ap.add_argument("--build-dir", default="build-tgl-shell-llvm",
                    help="Shell-enabled build directory (default: %(default)s)")
    ap.add_argument("--dut", default="spider",
                    help="DUT SSH hostname (default: %(default)s)")
    ap.add_argument("--nfs-root", default="/srv/nfs/spider-rootfs",
                    help="NFS rootfs path on this host (default: %(default)s)")
    ap.add_argument("--no-deploy", action="store_true",
                    help="Skip firmware copy to NFS rootfs")
    ap.add_argument("--no-reload", action="store_true",
                    help="Skip rmmod/modprobe on DUT")
    ap.add_argument("--cavstool", default="",
                    help="cavstool.py path on the DUT (default: auto-detect)")
    ap.add_argument("--timeout", type=int, default=4,
                    help="Per-command reply timeout in seconds (default: %(default)s)")
    args = ap.parse_args()

    # Change to workspace root (two levels up from sof/scripts/) so that
    # relative build-dir paths like 'build-tgl-shell-llvm' resolve correctly.
    script_dir = os.path.dirname(os.path.abspath(__file__))   # sof/scripts/
    sof_dir    = os.path.dirname(script_dir)                   # sof/
    workspace  = os.path.dirname(sof_dir)                      # workspace root
    os.chdir(workspace)

    print("=" * 60)
    print("SOF Shell Command Validator")
    print("=" * 60)

    if not args.no_deploy:
        deploy_firmware(args.build_dir, args.nfs_root)
    else:
        print("[deploy] Skipped.")

    if not args.no_reload:
        reload_firmware(args.dut)
    else:
        print("[reload] Skipped.")

    # Auto-detect cavstool on DUT if not given
    cavstool_path = args.cavstool
    if not cavstool_path:
        cavstool_path = find_cavstool_on_dut(args.dut)
        if cavstool_path:
            print(f"[cavstool] Found: {cavstool_path}")
        else:
            print("[cavstool] Warning: could not find cavstool.py on DUT; "
                  "the on-DUT snippet will try common locations.")

    # Build the on-DUT snippet by substituting parameters into the template
    snippet = _DUT_SNIPPET_TMPL.format(
        timeout=args.timeout,
        cavstool=cavstool_path,
        commands=COMMANDS,
        error_patterns=ERROR_PATTERNS,
    )

    print(f"\n[validate] Running {len(COMMANDS)} shell commands on {args.dut} ...")
    stdout, stderr = run_on_dut(args.dut, snippet)

    # Debug: always print raw DUT output to help diagnose issues
    if stderr.strip():
        print("[dut stderr]")
        for line in stderr.strip().splitlines():
            print("  " + line)
    if not stdout.strip():
        print("[error] DUT produced no stdout. Check above for errors.")
        sys.exit(1)

    # Parse JSON results
    results = None
    for line in stdout.splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            parsed = json.loads(line)
            if isinstance(parsed, list):
                results = parsed
                break
            if isinstance(parsed, dict) and "error" in parsed:
                sys.exit(f"[error] DUT reported: {parsed['error']}")
        except json.JSONDecodeError:
            pass

    if results is None:
        print("[error] Could not parse results from DUT.")
        print("DUT stdout was:")
        print(stdout)
        sys.exit(1)

    # Print report
    print()
    print("=" * 60)
    print(f"{'Command':<40} {'Result'}")
    print("-" * 60)
    passed = 0
    failed = 0
    for r in results:
        status = "PASS" if r["pass"] else "FAIL"
        print(f"  {r['cmd']:<38} {status}")
        if not r["pass"]:
            failed += 1
            # Show first 3 lines of output for context
            snippet_lines = [l for l in r["output"].splitlines() if l.strip()][:3]
            for line in snippet_lines:
                print(f"      > {line}")
        else:
            passed += 1

    print("-" * 60)
    print(f"  Total: {len(results)}   PASS: {passed}   FAIL: {failed}")
    print("=" * 60)

    sys.exit(0 if failed == 0 else 1)


if __name__ == "__main__":
    main()
